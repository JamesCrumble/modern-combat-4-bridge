package com.jamescrumble.mc4;

import android.content.Context;
import android.content.pm.PackageInfo;

import java.io.File;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Which build of the game the player has put in the folder, and which patch .obb goes with it.
 *
 * The version is the APK's own versionCode (the library inside is what runs). The patch .obb carries the
 * build number in its name (patch.12222.<package>.obb) and the engine reads whichever one it is given, so the
 * wrapper links only the matching one into game/obb.
 */
final class GameVersion {
    private static final Pattern PATCH = Pattern.compile("patch\\.(\\d+)\\..*\\.obb");

    /** One known build: its APK versionCode, the name, the patch it was released with and what sets it apart. */
    private static final class Build {
        final int code;
        final String name, patch, note;
        Build(int code, String name, String patch, String note) {
            this.code = code;
            this.name = name;
            this.patch = patch;
            this.note = note;
        }
    }

    private static final Build[] KNOWN = {
        new Build(12120, "1.2.1a", "11764",
                "предметы на уровнях (банки, коробки, компьютеры) разлетаются от выстрелов и попаданий. "
                        + "В 1.2.3e их вырезали."),
        new Build(12329, "1.2.3e", "12222",
                "последняя версия. Предметы на уровнях (банки, коробки, компьютеры) не разлетаются, "
                        + "они есть в 1.2.1a."),
    };

    /** The APK found in the game folder, or null. */
    final File apk;
    /** versionName from the APK, or null when it could not be read. */
    final String versionName;
    /** The known build, or null. */
    private final Build build;
    /** The patch .obb to use, or null when the folder has none (yet). */
    final File patch;

    private GameVersion(File apk, String versionName, Build build, File patch) {
        this.apk = apk;
        this.versionName = versionName;
        this.build = build;
        this.patch = patch;
    }

    static GameVersion detect(Context c) {
        File apk = Storage.firstWithSuffix(c, ".apk");
        String name = null;
        Build build = null;
        if (apk != null) {
            PackageInfo info = c.getPackageManager().getPackageArchiveInfo(apk.getPath(), 0);
            if (info != null) {
                name = info.versionName;
                for (Build b : KNOWN) if (b.code == info.versionCode) build = b;
            }
        }
        File dir = GameProcess.findObbAnywhere(c);
        return new GameVersion(apk, name, build, dir == null ? null : choosePatch(dir, build));
    }

    /**
     * The patch of the build when the folder has it, otherwise the newest one there is: a cache zip often brings
     * several, and the engine takes the single file it is given.
     */
    private static File choosePatch(File dir, Build build) {
        File best = null;
        long bestNumber = -1;
        File[] files = dir.listFiles();
        if (files == null) return null;
        for (File f : files) {
            Matcher m = PATCH.matcher(f.getName());
            if (!m.matches() || !f.isFile()) continue;
            if (build != null && m.group(1).equals(build.patch)) return f;
            long n = Long.parseLong(m.group(1));
            if (n > bestNumber) { best = f; bestNumber = n; }
        }
        return best;
    }

    /**
     * The folder (inside game/) the engine keeps its profile in. 1.2.3e keeps using the original "saves", so existing
     * progress stays where it was; any other version gets a folder of its own, because the profile (settings, control
     * layout, progress) is not laid out the same in every build and one reading the other's file mixes them up.
     */
    String saveFolder() {
        if (versionName == null || versionName.equals("1.2.3e")) return "saves";
        return "saves-" + versionName.replaceAll("[^A-Za-z0-9._-]", "_");
    }

    /** Is the patch the one this build was released with? True also when the build or the patch is unknown. */
    boolean patchMatches() {
        if (build == null || patch == null) return true;
        Matcher m = PATCH.matcher(patch.getName());
        return m.matches() && m.group(1).equals(build.patch);
    }

    /** The line for the settings screen, or null when there is no APK to speak about. */
    String notice() {
        if (apk == null) return null;
        if (build == null) {
            return "Версия игры" + (versionName != null ? " " + versionName : "") + " не распознана. "
                    + "Порт проверен на 1.2.3e и 1.2.1a.";
        }
        String s = "Обнаружена " + build.name + ": " + build.note;
        if (!patchMatches()) {
            s += "\nКэш от другой версии: нужен patch." + build.patch + ", лежит " + patch.getName().split("\\.")[1] + ".";
        }
        return s;
    }
}
