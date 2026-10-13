package com.jamescrumble.mc4;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.os.SystemClock;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.system.StructPollfd;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;

/**
 * Owns everything outside the UI: unpacking the sysroot, linking the player's
 * game files, running qemu-arm and talking to the port through the control
 * directory (commands in, frame.bin and status.json out, audio over a FIFO).
 */
final class GameProcess {
    interface Listener {
        void onStatus(String text);
        void onExit(int code);
    }

    private static final String NATIVE_LIB = "lib/armeabi-v7a/libModernCombat4.so";
    /** The game's own package: the name of the folder the cache archive carries. */
    private static final String GAME_PACKAGE = "com.gameloft.android.ANMP.GloftM4HM";
    /** The main expansion file: the engine reads its data straight out of it (and out of the patch one next to it). */
    private static final String MAIN_OBB = "main.1100." + GAME_PACKAGE + ".obb";
    private static final int SAMPLE_RATE = 44100;

    private static final class Cmd {
        String line;
        final String key;
        Cmd(String line, String key) { this.line = line; this.key = key; }
    }

    private final Context ctx;
    private volatile Listener listener;
    private final boolean touchMode;
    private final File root;
    private final File control;
    private final File commandsFile;
    private final File statusFile;
    private final File fifo;
    private final File logFile;
    private final LinkedList<Cmd> queue = new LinkedList<>();
    private volatile boolean running;
    private volatile boolean muted = true;
    private Process process;
    private final GlBridge gl;
    private final PerfHint perf;
    private int renderW = 1280, renderH = 590;

    GameProcess(Context ctx, Listener listener) {
        this.ctx = ctx;
        this.listener = listener;
        root = ctx.getFilesDir();
        control = new File(root, "ctl");
        commandsFile = new File(control, "commands");
        statusFile = new File(control, "status.json");
        fifo = new File(root, "audio.pcm");
        File external = ctx.getExternalFilesDir(null);
        logFile = new File(external != null ? external : root, "mc4.log");
        // pad_overlay.flag next to the log brings back the wrapper's own gamepad-style overlay.
        touchMode = !new File(logFile.getParentFile(), "pad_overlay.flag").exists();
        perf = new PerfHint(ctx, msg -> appendLog("[perf] " + msg));
        gl = new GlBridge(new File(root, "gl_cmd.pipe").getPath(), new File(root, "gl_reply.pipe").getPath(),
                msg -> appendLog("[gl] " + msg), perf::frame, perf::cpuLine);
        chooseRenderSize();
    }

    /**
     * The engine renders at whatever size it is told, so use the phone's own aspect ratio and let the
     * compositor scale the surface to the panel. resolution.txt ("1280x590") next to the log overrides it.
     */
    private void chooseRenderSize() {
        android.util.DisplayMetrics m = ctx.getResources().getDisplayMetrics();
        int longSide = Math.max(m.widthPixels, m.heightPixels), shortSide = Math.min(m.widthPixels, m.heightPixels);
        int h = 720;
        try { h = Integer.parseInt(SettingsActivity.load(ctx).getOrDefault("RENDER_HEIGHT", "720")); } catch (NumberFormatException ignored) { }
        h = Math.max(240, Math.min(h, 2160)) & ~1;
        int w = Math.round(h * (float) longSide / shortSide) & ~1;
        String over = readText(new File(logFile.getParentFile(), "resolution.txt")).trim();
        java.util.regex.Matcher mt = java.util.regex.Pattern.compile("(\\d+)x(\\d+)").matcher(over);
        if (mt.matches()) {
            w = Integer.parseInt(mt.group(1));
            h = Integer.parseInt(mt.group(2));
        }
        renderW = Math.max(320, Math.min(w, 4096));
        renderH = Math.max(240, Math.min(h, 4096));
    }

    int renderWidth() { return renderW; }
    int renderHeight() { return renderH; }
    GlBridge gl() { return gl; }
    File logFile() { return logFile; }
    boolean isRunning() { return running; }
    boolean touchMode() { return touchMode; }
    void setListener(Listener l) { listener = l; }
    void setMuted(boolean m) { muted = m; }

    void start() throws Exception {
        stopStale();
        prepare();
        launch();
    }

    /** Returns {frame, menu}, or null before the port has written a status. */
    int[] readStatus() {
        String s = readText(statusFile);
        if (s.isEmpty()) return null;
        return new int[] { intField(s, "\"frame\":"), intField(s, "\"menu\":") };
    }

    /**
     * Queues one command line. Commands with a key (cursor, stick) replace an
     * older queued command of the same key, because the port consumes only one
     * line per frame and stale positions would only add lag.
     */
    void command(String line, String key) {
        synchronized (queue) {
            if (!running) return;
            if (key != null) {
                ListIterator<Cmd> it = queue.listIterator(queue.size());
                while (it.hasPrevious()) {
                    Cmd c = it.previous();
                    if (c.key == null) break;
                    if (c.key.equals(key)) { c.line = line; return; }
                }
            }
            queue.add(new Cmd(line, key));
            queue.notifyAll();
        }
    }

    void stop() {
        boolean wasRunning = running;
        running = false;
        gl.stop();
        synchronized (queue) { queue.notifyAll(); }
        if (!wasRunning) return;
        appendLine("quit");
        final Process p = process;
        if (p != null) {
            new Thread(() -> {
                try {
                    if (!p.waitFor(2, TimeUnit.SECONDS)) p.destroy();
                } catch (InterruptedException ignored) {
                    p.destroy();
                }
            }, "mc4-stop").start();
        }
    }

    // A qemu left behind by a killed app would keep running on its own;
    // ask it to quit before the control files are recreated.
    private void stopStale() throws InterruptedException {
        if (!statusFile.exists() || System.currentTimeMillis() - statusFile.lastModified() > 5000) return;
        appendLine("quit");
        long end = System.currentTimeMillis() + 5000;
        while (System.currentTimeMillis() < end && !readText(statusFile).contains("\"stopped\"")) {
            Thread.sleep(100);
        }
    }

    private void prepare() throws Exception {
        listener.onStatus("Подготовка файлов…");
        File bin = new File(root, "bin");
        File sysroot = new File(root, "sysroot");

        String stamp = String.valueOf(ctx.getPackageManager().getPackageInfo(ctx.getPackageName(), 0).lastUpdateTime);
        File stampFile = new File(root, "install.stamp");
        if (!stamp.equals(readText(stampFile).trim())) {
            deleteTree(bin);
            deleteTree(sysroot);
            stampFile.delete();
        }

        File binary = new File(bin, "mc4");
        if (!binary.exists()) {
            bin.mkdirs();
            try (InputStream in = new BufferedInputStream(ctx.getAssets().open("mc4"))) {
                copy(in, new FileOutputStream(binary));
            }
        }

        File done = new File(sysroot, ".done");
        if (!done.exists()) {
            listener.onStatus("Распаковка системных библиотек…\nЭто делается один раз.");
            unzipAsset("sysroot.zip");
            done.createNewFile();
        }
        writeText(stampFile, stamp);
        // qemu-user refuses to load a guest program whose file lacks the execute bit.
        binary.setExecutable(true, true);
        new File(sysroot, "lib/ld-linux-armhf.so.3").setExecutable(true, true);

        File game = new File(root, "game");
        File libDir = new File(game, "lib/armeabi-v7a");
        libDir.mkdirs();

        File external = ctx.getExternalFilesDir(null);
        String where = external == null ? "Android/data/" + ctx.getPackageName() + "/files" : external.getPath();
        File apk = external == null ? null : firstWithSuffix(external, ".apk");
        if (apk == null) {
            throw new IOException("Не найден оригинальный APK игры (v1.2.3e).\nПоложите его в папку:\n" + where);
        }
        listener.onStatus("Извлечение libModernCombat4.so…");
        extractNative(apk, new File(libDir, "libModernCombat4.so"));
        // The engine asks Java for files out of the APK's assets and opens the APK itself as a zip.
        extractAssets(apk, new File(game, "assets"));
        relink(new File(game, "game.apk"), apk);
        // The folder the engine takes for the SD card one: it looks for loose data files there before the OBB.
        new File(game, "files").mkdirs();

        // The two .obb files stay where the player put them; the port reads them through game/obb.
        File obb = external == null ? null : findObb(external);
        if (obb == null && external != null) {
            // Simplest for the player: the cache archive as a zip next to the game APK, unpacked here once.
            File zip = firstWithSuffix(external, ".zip");
            if (zip != null) {
                extractCache(zip, new File(external, GAME_PACKAGE));
                obb = findObb(external);
            }
        }
        if (obb == null) {
            File locked = external == null ? null : firstUnreadable(new File(external, GAME_PACKAGE));
            if (locked != null) {
                // Folders made by "adb push" belong to the shell user and are closed to the app.
                throw new IOException("Кэш игры лежит в папке, но приложению закрыт доступ к ней:\n" + locked.getPath()
                        + "\nПапки создал adb. Откройте их командой:\nadb shell \"find " + new File(external, GAME_PACKAGE).getPath()
                        + " -type d -exec chmod 2777 {} +\"");
            }
            // A folder the app makes itself stays open to it, whoever copies the files in afterwards.
            new File(external, GAME_PACKAGE).mkdirs();
            throw new IOException("Не найден кэш игры (файл " + MAIN_OBB + ").\nПоложите zip-архив с кэшем рядом с APK игры, в папку:\n" + where
                    + "\nЛибо положите два файла .obb в уже созданную папку:\n" + where + "/" + GAME_PACKAGE);
        }
        relink(new File(game, "obb"), obb);
        appendLog("[wrapper] game data: " + obb.getPath());
    }

    /** Makes link a symlink to target, replacing whatever was there. */
    private static void relink(File link, File target) throws IOException {
        String now = "";
        try { now = Os.readlink(link.getPath()); } catch (ErrnoException ignored) { }
        if (now.equals(target.getPath())) return;
        deleteTree(link);
        link.delete();
        try {
            Os.symlink(target.getPath(), link.getPath());
        } catch (ErrnoException e) {
            throw new IOException("Не удалось подключить " + target.getName() + ": " + e.getMessage());
        }
    }

    /** The first folder under dir (dir included) that exists and cannot be listed, or null. */
    private static File firstUnreadable(File dir) {
        if (!dir.isDirectory()) return null;
        File[] children = dir.listFiles();
        if (children == null) return dir;
        for (File c : children) {
            File f = firstUnreadable(c);
            if (f != null) return f;
        }
        return null;
    }

    /** The folder holding the .obb files: the archive's own layout, or the files put there directly. */
    private static File findObb(File external) {
        File[] candidates = {
            new File(external, GAME_PACKAGE),
            external,
            new File(external, "obb"),
            new File(external, "obb/" + GAME_PACKAGE),
        };
        for (File c : candidates) if (new File(c, MAIN_OBB).isFile()) return c;
        return null;
    }

    private void launch() throws Exception {
        listener.onStatus("Запуск игры…");
        File qemu = new File(ctx.getApplicationInfo().nativeLibraryDir, "libqemu.so");
        if (!qemu.exists()) throw new IOException("Нет libqemu.so в " + qemu.getParent());

        control.mkdirs();
        statusFile.delete();
        commandsFile.delete();
        commandsFile.createNewFile();
        fifo.delete();
        Os.mkfifo(fifo.getPath(), 0600);
        File glCmd = new File(root, "gl_cmd.pipe"), glReply = new File(root, "gl_reply.pipe");
        glCmd.delete();
        glReply.delete();
        Os.mkfifo(glCmd.getPath(), 0600);
        Os.mkfifo(glReply.getPath(), 0600);
        logFile.delete();
        gl.start();

        ProcessBuilder b = new ProcessBuilder(qemu.getPath());
        // Diagnostics: extra qemu arguments, one per line, from qemu_args.txt next to the log.
        for (String arg : readText(new File(logFile.getParentFile(), "qemu_args.txt")).split("\n")) {
            if (!arg.trim().isEmpty()) b.command().add(arg.trim());
        }
        // Android hides /proc/sys/vm/mmap_min_addr, so qemu would try to reserve guest memory at
        // 0x1000 and be refused; a non-zero guest base sidesteps that.
        // The default "any" CPU makes qemu emulate NEON/VFP through slow helpers; a real Cortex-A15
        // profile is several times faster on the game's own code.
        b.command().add("-cpu");
        b.command().add("cortex-a15");
        b.command().add("-B");
        b.command().add("0x100000000");
        b.command().add("-L");
        b.command().add(new File(root, "sysroot").getPath());
        b.command().add(new File(root, "bin/mc4").getPath());
        b.command().add(new File(root, "game").getPath());
        Map<String, String> env = b.environment();
        env.put("HOME", new File(root, "home").getPath());
        env.put("SDL_VIDEODRIVER", "offscreen");
        env.put("SDL_AUDIODRIVER", "disk");
        env.put("SDL_DISKAUDIOFILE", fifo.getPath());
        // The port's AudioTrack class writes the game's mixer blocks straight into the FIFO (see audioLoop()).
        env.put("MC4_AUDIO_FIFO", fifo.getPath());
        env.put("MC4_CONTROL_DIR", control.getPath());
        // OpenGL ES calls are executed by the app on the GPU (GlBridge); nothing renders inside qemu.
        env.put("MC4_GL_BRIDGE", "1");
        env.put("MC4_GL_CMD", glCmd.getPath());
        env.put("MC4_GL_REPLY", glReply.getPath());
        env.put("MC4_LANG", java.util.Locale.getDefault().getLanguage());
        // The engine has a frame limiter of its own, fixed at 60; it follows the menu's frame limit.
        env.put("MC4_WIDTH", String.valueOf(renderW));
        env.put("MC4_HEIGHT", String.valueOf(renderH));
        env.put("MC4_FB_PROBE_INTERVAL", "0");
        env.put("MC4_CURSOR", "0");
        // An ordinary touch phone to the engine: its own on-screen controls, no PowerA pad until one is used.
        if (touchMode) env.put("MC4_TOUCH", "1");
        // The engine sizes its quality profile by GPU name; gpu_name.txt next to the log overrides the real one.
        String gpu = readText(new File(logFile.getParentFile(), "gpu_name.txt")).trim();
        if (!gpu.isEmpty()) env.put("MC4_GL_RENDERER", gpu);
        File dir = logFile.getParentFile();
        // engine_env.txt: KEY=VALUE per line, added to the port's environment.
        for (String line : readText(new File(dir, "engine_env.txt")).split("\n")) {
            int eq = line.indexOf('=');
            if (eq > 0 && !line.trim().startsWith("#")) env.put(line.substring(0, eq).trim(), line.substring(eq + 1).trim());
        }
        // settings.txt is written by SettingsActivity and wins over engine_env.txt (RENDER_HEIGHT is ours, not the engine's).
        for (java.util.Map.Entry<String, String> e : SettingsActivity.load(ctx).entrySet())
            if (!e.getKey().equals("RENDER_HEIGHT") && !e.getKey().equals("CPU_PRIME")
                    && !e.getKey().equals("AUDIO_LATENCY_MS") && !e.getKey().equals("PERF_HINT")) env.put(e.getKey(), e.getValue());
        // Our qemu: guest syscall counts every 10 s in the log (which ones leave translated code most often).
        env.put("QEMU_SYSCALL_STATS", "1");
        // Guest-function profile: qemu samples the game's PC (tools/guest_prof.py maps it to functions).
        // engine_env.txt with QEMU_GUEST_PROF=0 turns it off.
        env.put("QEMU_GUEST_PROF", "1");
        // glibc 2.35+ registers rseq at startup; Android's seccomp filter may kill the process for it.
        env.put("GLIBC_TUNABLES", "glibc.pthread.rseq=0");
        File home = new File(root, "home");
        home.mkdirs();
        // Loader trace stays on while the Android build is being brought up; add notrace.flag next to the log to mute it.
        if (!new File(logFile.getParentFile(), "notrace.flag").exists()) env.put("LOADER_TRACE", "1");
        // Diagnostics: run diag_cmd.txt with sh instead of the game, same uid and environment.
        File diag = new File(logFile.getParentFile(), "diag_cmd.txt");
        if (diag.exists()) {
            env.put("QEMU", qemu.getPath());
            env.put("STRACE", new File(ctx.getApplicationInfo().nativeLibraryDir, "libmstrace.so").getPath());
            env.put("ROOT", root.getPath());
            b.command("/system/bin/sh", "-c", readText(diag));
        }
        // cpu_mask.txt next to the log (a hex mask, e.g. "c0" = cores 6-7): qemu and all its threads stay on those
        // cores. The frequency limiter caps clusters differently from moment to moment, and the scheduler keeps
        // moving the game's one busy thread onto whichever is free - often a capped one.
        String mask = readText(new File(logFile.getParentFile(), "cpu_mask.txt")).trim();
        // On by default: in the first session the game's main thread sat on the middle cores at 1.4-1.8 GHz.
        if ("1".equals(SettingsActivity.load(ctx).getOrDefault("CPU_PRIME", "1"))) mask = "c0";
        if (diag.exists()) mask = "";
        if (mask.matches("[0-9a-fA-F]{1,4}")) {
            b.command().add(0, "/system/bin/taskset");
            b.command().add(1, mask);
            appendLog("[wrapper] qemu pinned to cpu mask 0x" + mask);
        }
        b.redirectErrorStream(true);
        b.redirectOutput(ProcessBuilder.Redirect.appendTo(logFile));

        running = true;
        final long startedAt = SystemClock.elapsedRealtime();
        appendLog("[wrapper] " + b.command());
        process = b.start();
        final Process p = process;
        int fpsLimit = 60;
        try { fpsLimit = Integer.parseInt(SettingsActivity.load(ctx).getOrDefault("FPS_LIMIT", "60")); } catch (NumberFormatException ignored) { }
        gl.setFrameLimit(fpsLimit);
        perf.start(p, "1".equals(SettingsActivity.load(ctx).get("PERF_HINT")), fpsLimit);
        new Thread(this::writerLoop, "mc4-commands").start();
        new Thread(this::audioLoop, "mc4-audio").start();
        new Thread(() -> {
            int code = -1;
            try { code = p.waitFor(); } catch (InterruptedException ignored) { }
            appendLog("[wrapper] qemu exited with code " + code
                    + (code > 128 ? " (signal " + (code - 128) + ")" : "")
                    + " after " + (SystemClock.elapsedRealtime() - startedAt) + " ms");
            if (running) {
                running = false;
                listener.onExit(code);
            }
        }, "mc4-watch").start();
    }

    /**
     * Writes whatever is queued in one go. The port paces the lines itself (touch lines several per
     * frame, everything else one per frame), so the pause here only lets moves coalesce.
     */
    private void writerLoop() {
        StringBuilder batch = new StringBuilder();
        try (FileOutputStream out = new FileOutputStream(commandsFile, true)) {
            while (running) {
                batch.setLength(0);
                synchronized (queue) {
                    while (queue.isEmpty() && running) queue.wait();
                    if (!running) break;
                    for (Cmd c : queue) batch.append(c.line).append('\n');
                    queue.clear();
                }
                out.write(batch.toString().getBytes("US-ASCII"));
                Thread.sleep(4);
            }
        } catch (Exception ignored) {
        }
    }

    /**
     * Plays what the port writes into the FIFO (its AudioTrack class, one mixer block per write).
     *
     * The AudioTrack is the clock. The pipe is shrunk to one page and the track buffer to the menu's
     * AUDIO_LATENCY_MS (the device minimum by default), so the port's writer blocks until the speaker has
     * consumed what came before and the game's mixer is called back at exactly that rate. With a 64 KB pipe
     * and a 64 KB track buffer, and a writer paced by its own sleep, the sound drifted most of a second late.
     */
    private void audioLoop() {
        int min = AudioTrack.getMinBufferSize(SAMPLE_RATE, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_16BIT);
        // 0 (the default) is the smallest buffer the device allows.
        int latencyMs = 0;
        try { latencyMs = Integer.parseInt(SettingsActivity.load(ctx).getOrDefault("AUDIO_LATENCY_MS", "0")); } catch (NumberFormatException ignored) { }
        int bufBytes = Math.max(min, (SAMPLE_RATE * 4 * Math.max(0, Math.min(latencyMs, 1000)) / 1000) & ~3);
        AudioTrack track = new AudioTrack.Builder()
                .setAudioAttributes(new AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_GAME)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC).build())
                .setAudioFormat(new AudioFormat.Builder()
                        .setSampleRate(SAMPLE_RATE)
                        .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT).build())
                .setBufferSizeInBytes(bufBytes)
                .setPerformanceMode(AudioTrack.PERFORMANCE_MODE_LOW_LATENCY)
                .setTransferMode(AudioTrack.MODE_STREAM)
                .build();
        FileDescriptor fd = null;
        try {
            // O_RDWR keeps the FIFO open without a writer yet and never reports EOF,
            // so the loop can poll with a timeout and notice when the game stops.
            fd = Os.open(fifo.getPath(), OsConstants.O_RDWR | OsConstants.O_NONBLOCK, 0);
            int pipe = -1;
            if (android.os.Build.VERSION.SDK_INT >= 30) {
                try { pipe = Os.fcntlInt(fd, 1031 /* F_SETPIPE_SZ */, 4096); } catch (ErrnoException ignored) { }
            }
            appendLog("[audio] track buffer " + bufBytes + " bytes (" + bufBytes * 1000L / (SAMPLE_RATE * 4) + " ms, min " + min
                    + "), pipe " + pipe + " bytes");
            StructPollfd[] polls = { new StructPollfd() };
            polls[0].fd = fd;
            polls[0].events = (short) OsConstants.POLLIN;
            byte[] buf = new byte[16384];
            boolean playing = false;
            int underruns = 0;
            long nextReport = SystemClock.elapsedRealtime() + 10000;
            while (running) {
                polls[0].revents = 0;
                if (Os.poll(polls, 100) <= 0) continue;
                int n;
                try {
                    n = Os.read(fd, buf, 0, buf.length);
                } catch (ErrnoException e) {
                    if (e.errno == OsConstants.EAGAIN) continue;
                    break;
                } catch (InterruptedIOException e) {
                    break;
                }
                if (n <= 0) continue;
                // While the game is loading (or the app is in the background) its audio is replaced by
                // silence rather than dropped: the track has to stay the game mixer's clock.
                if (muted) java.util.Arrays.fill(buf, 0, n, (byte) 0);
                if (!playing) { track.play(); playing = true; }
                track.write(buf, 0, n);
                long now = SystemClock.elapsedRealtime();
                if (now >= nextReport) {
                    nextReport = now + 10000;
                    int u = track.getUnderrunCount();
                    if (u != underruns) appendLog("[audio] speaker ran dry " + (u - underruns) + " time(s) in 10 s");
                    underruns = u;
                }
            }
        } catch (ErrnoException ignored) {
        } finally {
            if (fd != null) try { Os.close(fd); } catch (ErrnoException ignored) { }
            track.release();
        }
    }

    private void unzipAsset(String name) throws IOException {
        String rootPath = root.getCanonicalPath() + File.separator;
        try (ZipInputStream z = new ZipInputStream(new BufferedInputStream(ctx.getAssets().open(name)))) {
            ZipEntry e;
            while ((e = z.getNextEntry()) != null) {
                File out = new File(root, e.getName());
                if (!out.getCanonicalPath().startsWith(rootPath)) throw new IOException("bad zip entry " + e.getName());
                if (e.isDirectory()) { out.mkdirs(); continue; }
                out.getParentFile().mkdirs();
                copy(z, new FileOutputStream(out), false);
            }
        }
    }

    private void extractNative(File apk, File out) throws IOException {
        try (ZipFile z = new ZipFile(apk)) {
            ZipEntry e = z.getEntry(NATIVE_LIB);
            if (e == null) throw new IOException("В APK нет " + NATIVE_LIB + ".\nНужна версия 1.2.3e.");
            if (out.exists() && out.length() == e.getSize()) return;
            copy(z.getInputStream(e), new FileOutputStream(out));
        }
    }

    void log(String line) { appendLog("[app] " + line); }

    private void appendLog(String line) {
        try (FileOutputStream out = new FileOutputStream(logFile, true)) {
            out.write((line + "\n").getBytes("UTF-8"));
        } catch (IOException ignored) {
        }
    }

    private void appendLine(String line) {
        try (FileOutputStream out = new FileOutputStream(commandsFile, true)) {
            out.write((line + "\n").getBytes("US-ASCII"));
        } catch (IOException ignored) {
        }
    }

    /**
     * A cache zip into dir, from the game's own folder down: whatever folders the archive wraps it in are
     * dropped. A main.*.obb inside is skipped: where an archive carries one, it is the same data once more
     * in a single file.
     */
    /** Unpacks the .obb files of the cache archive into dir, whatever folders the archive wraps them in. */
    private void extractCache(File zip, File dir) throws IOException {
        long total = 0, done = 0, shown = -1;
        int count = 0;
        if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException("mkdir " + dir);
        try (ZipFile z = new ZipFile(zip)) {
            java.util.Enumeration<? extends ZipEntry> en = z.entries();
            while (en.hasMoreElements()) {
                ZipEntry e = en.nextElement();
                if (isObbEntry(e)) total += Math.max(0, e.getSize());
            }
            en = z.entries();
            byte[] buf = new byte[1 << 20];
            while (en.hasMoreElements()) {
                ZipEntry e = en.nextElement();
                if (!isObbEntry(e)) continue;
                String n = e.getName();
                File out = new File(dir, n.substring(n.lastIndexOf('/') + 1));
                File part = new File(dir, out.getName() + ".part");
                try (InputStream in = z.getInputStream(e); OutputStream os = new FileOutputStream(part)) {
                    for (int r; (r = in.read(buf)) > 0; ) {
                        os.write(buf, 0, r);
                        done += r;
                        long pct = total > 0 ? done * 100 / total : 0;
                        if (pct != shown) {
                            shown = pct;
                            listener.onStatus("Распаковка кэша: " + pct + "%");
                        }
                    }
                }
                // Renamed when complete: an unpacking cut short must not look like a finished file.
                if (!part.renameTo(out)) throw new IOException("rename " + part);
                count++;
            }
        }
        if (count == 0) throw new IOException("В zip нет файлов .obb\nНужен кэш игры 1.2.3e: " + zip.getName());
    }

    /** A real .obb of the archive (macOS archives carry "._name.obb" resource forks beside them). */
    private static boolean isObbEntry(ZipEntry e) {
        String n = e.getName();
        String leaf = n.substring(n.lastIndexOf('/') + 1);
        return !e.isDirectory() && leaf.endsWith(".obb") && !leaf.startsWith("._") && !n.startsWith("__MACOSX/");
    }

    /** Unpacks the APK's assets/ folder into dir (a handful of small files). */
    private void extractAssets(File apk, File dir) throws IOException {
        String base = dir.getCanonicalPath() + File.separator;
        try (ZipFile z = new ZipFile(apk)) {
            java.util.Enumeration<? extends ZipEntry> en = z.entries();
            while (en.hasMoreElements()) {
                ZipEntry e = en.nextElement();
                String n = e.getName();
                if (e.isDirectory() || !n.startsWith("assets/")) continue;
                File out = new File(dir, n.substring("assets/".length()));
                if (!out.getCanonicalPath().startsWith(base)) throw new IOException("bad zip entry " + n);
                if (out.isFile() && out.length() == e.getSize()) continue;
                File parent = out.getParentFile();
                if (parent != null && !parent.isDirectory() && !parent.mkdirs()) throw new IOException("mkdir " + parent);
                try (InputStream in = z.getInputStream(e)) {
                    copy(in, new FileOutputStream(out));
                }
            }
        }
    }

    private static File firstWithSuffix(File dir, String suffix) {
        File[] files = dir.listFiles((d, n) -> n.endsWith(suffix));
        if (files == null || files.length == 0) return null;
        Arrays.sort(files);
        return files[0];
    }

    private static int intField(String s, String key) {
        int i = s.indexOf(key);
        if (i < 0) return 0;
        i += key.length();
        int j = i;
        while (j < s.length() && Character.isDigit(s.charAt(j))) j++;
        try { return Integer.parseInt(s.substring(i, j)); } catch (NumberFormatException e) { return 0; }
    }

    private static String readText(File f) {
        if (!f.isFile()) return "";
        try (FileInputStream in = new FileInputStream(f)) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[4096];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            return out.toString("UTF-8");
        } catch (IOException e) {
            return "";
        }
    }

    private static void writeText(File f, String text) throws IOException {
        try (FileOutputStream out = new FileOutputStream(f)) { out.write(text.getBytes("UTF-8")); }
    }

    private static void copy(InputStream in, OutputStream out) throws IOException {
        try (InputStream i = in) { copy(i, out, true); }
    }

    private static void copy(InputStream in, OutputStream out, boolean unused) throws IOException {
        byte[] buf = new byte[65536];
        int n;
        try {
            while ((n = in.read(buf)) >= 0) if (n > 0) out.write(buf, 0, n);
        } finally {
            out.close();
        }
    }

    private static void deleteTree(File f) {
        File[] children = f.listFiles();
        if (children != null) for (File c : children) deleteTree(c);
        f.delete();
    }
}
