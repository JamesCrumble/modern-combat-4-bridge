package com.jamescrumble.mc4;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Game settings. They are kept in settings.txt (KEY=VALUE per line) next to the game log; GameProcess
 * adds them to the engine's environment after engine_env.txt, so a menu value wins over a hand-edited one.
 * This is the app's launcher screen: "Играть" saves and starts the game (the game process is restarted
 * so that it always begins with the values shown here).
 */
public final class SettingsActivity extends Activity {
    private static final int BG = Color.rgb(18, 22, 26), FG = Color.rgb(230, 235, 238), DIM = Color.rgb(140, 150, 156),
            ACCENT = Color.rgb(128, 203, 196);

    private final Map<String, String> values = new LinkedHashMap<>();
    private float dp;
    private TextView folderInfo;
    private boolean hadAccess, asked;

    /** The existing settings file (main folder first, then the other), or where it would be written. */
    static File file(Context c) {
        return Storage.find(c, "settings.txt");
    }

    static Map<String, String> load(Context c) {
        Map<String, String> m = new LinkedHashMap<>();
        File f = file(c);
        if (!f.isFile()) return m;
        try (FileInputStream in = new FileInputStream(f)) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] b = new byte[2048];
            int n;
            while ((n = in.read(b)) > 0) out.write(b, 0, n);
            for (String line : out.toString("UTF-8").split("\n")) {
                int eq = line.indexOf('=');
                if (eq > 0) m.put(line.substring(0, eq).trim(), line.substring(eq + 1).trim());
            }
        } catch (IOException ignored) { }
        return m;
    }

    private static void write(Context c, Map<String, String> m) throws IOException {
        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String> e : m.entrySet()) sb.append(e.getKey()).append('=').append(e.getValue()).append('\n');
        try (FileOutputStream out = new FileOutputStream(new File(Storage.mainDir(c), "settings.txt"))) {
            out.write(sb.toString().getBytes("UTF-8"));
        }
    }

    /** Removes a one-shot value from the saved settings once the game has taken it. */
    static void consume(Context c, String key) {
        Map<String, String> m = load(c);
        if (m.remove(key) == null) return;
        try { write(c, m); } catch (IOException ignored) { }
    }

    private boolean save() {
        try {
            write(this, values);
            return true;
        } catch (IOException e) {
            Toast.makeText(this, "Не удалось сохранить: " + e.getMessage(), Toast.LENGTH_LONG).show();
            return false;
        }
    }

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        dp = getResources().getDisplayMetrics().density;
        values.putAll(load(this));

        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setPadding(px(20), px(16), px(20), px(24));

        col.addView(text("Modern Combat 4 — настройки", 22, FG, true));
        col.addView(text("Настрой и нажми «Играть» — игра стартует с этими значениями.", 13, DIM, false));
        folderInfo = text("", 12, DIM, false);
        folderInfo.setPadding(0, px(6), 0, 0);
        col.addView(folderInfo);

        col.addView(section("Картинка"));
        col.addView(choice("Разрешение рендера (высота)", "RENDER_HEIGHT", new int[] { 480, 540, 720 }, 720, "p",
                "Ширина считается по пропорциям экрана. Ниже — быстрее."));

        col.addView(choice("Лимит кадров", "FPS_LIMIT", new int[] { 30, 60, 90, 120 }, 60, " fps",
                "Целевая частота экрана и верхняя граница частоты кадров игры."));
        col.addView(toggle("Счётчик FPS", "FPS_COUNTER", true, "Маленькая зелёная цифра вверху по центру экрана."));

        col.addView(section("Деньги"));
        col.addView(toggle("Бесконечные деньги", "MC4_MONEY_INFINITE", false,
                "Кредиты показываются как 9 999 999, покупки ничего не списывают. Настоящий баланс не трогается: "
                        + "выключишь — вернётся то, что было, плюс заработанное."));
        col.addView(amount("Задать кредиты", "MC4_MONEY_RESTORE", 9999999,
                "Впиши сумму и нажми «Играть»: когда игра загрузит профиль, кредиты станут этой суммой (один раз). "
                        + "При включённых бесконечных деньгах не действует."));

        col.addView(section("Звук"));
        col.addView(choice("Буфер звука", "AUDIO_LATENCY_MS", new int[] { 0, 200, 400 }, 0, " мс",
                "0 — минимальный для телефона, наименьшая задержка. Больше — если звук прерывается в тяжёлых сценах."));

        col.addView(section("Производительность"));
        col.addView(choice("Кадров «в полёте»", "MC4_GL_INFLIGHT", new int[] { 1, 2, 3 }, 2, "",
                "Сколько кадров игра готовит вперёд, пока телефон рисует предыдущий. 2 — плавнее, 1 — меньше задержка."));
        col.addView(toggle("Только быстрые ядра (6-7)", "CPU_PRIME", true,
                "Держит игру на двух самых быстрых ядрах: система реже переносит главный поток на медленные."));
        col.addView(toggle("Подсказки производительности системе (ADPF)", "PERF_HINT", false,
                "Сообщает Android время кадра."));
        col.addView(toggle("Профилирование в логе", "QEMU_GUEST_PROF", true,
                "Раз в 10 секунд пишет в лог, где игра тратит время. Для отладки."));

        ScrollView sv = new ScrollView(this);
        sv.addView(col, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        // The buttons stay at the bottom whatever the scroll position.
        Button reset = button("Вернуть как было");
        reset.setOnClickListener(v -> {
            values.clear();
            Storage.deleteEverywhere(this, "settings.txt");
            recreate();
        });
        Button saveOnly = button("Сохранить");
        saveOnly.setOnClickListener(v -> {
            if (save()) Toast.makeText(this, "Сохранено", Toast.LENGTH_SHORT).show();
        });
        Button play = button("Играть");
        play.setOnClickListener(v -> { if (save()) restartGame(); });
        LinearLayout bar = new LinearLayout(this);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setPadding(px(16), px(4), px(16), px(8));
        for (Button bt : new Button[] { reset, saveOnly, play }) {
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
            lp.setMargins(px(4), 0, px(4), 0);
            bar.addView(bt, lp);
        }
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        root.addView(sv, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));
        root.addView(bar, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        setContentView(root);
        hadAccess = Storage.hasAccess(this);
        showFolder();
        if (!hadAccess) askAccess();
        else moveFiles();
    }

    @Override protected void onResume() {
        super.onResume();
        // Back from the system page: if the access has just been given, take the files over.
        boolean now = Storage.hasAccess(this);
        if (now && !hadAccess) moveFiles();
        hadAccess = now;
        showFolder();
    }

    @Override public void onRequestPermissionsResult(int code, String[] perms, int[] results) {
        if (Storage.hasAccess(this)) { hadAccess = true; moveFiles(); }
        showFolder();
    }

    private void showFolder() {
        if (folderInfo == null) return;
        File main = Storage.mainDir(this);
        folderInfo.setText("Папка игры: " + main.getPath() + (Storage.hasAccess(this) ? ""
                : "\n(нет доступа ко всем файлам — используется папка приложения в Android/data)"));
    }

    /** Asks once per launch of this screen for access to all files; the game works without it. */
    private void askAccess() {
        if (asked) return;
        asked = true;
        new AlertDialog.Builder(this)
                .setTitle("Доступ ко всем файлам")
                .setMessage("Чтобы хранить APK игры, кэш и сохранения в удобной папке\n"
                        + Storage.shared(this).getPath() + "\nприложению нужен доступ ко всем файлам. Файлы из Android/data "
                        + "будут перенесены туда сами. Без доступа игра работает с папкой в Android/data.")
                .setPositiveButton("Разрешить", (d, w) -> Storage.requestAccess(this, 2))
                .setNegativeButton("Позже", null)
                .show();
    }

    /** Creates BridgeGames/<package> and moves the files of Android/data into it (off the UI thread: it may copy). */
    private void moveFiles() {
        new Thread(() -> {
            int n = Storage.migrate(getApplicationContext(), t -> runOnUiThread(() -> folderInfo.setText(t)));
            runOnUiThread(() -> {
                showFolder();
                if (n > 0) Toast.makeText(this, "Файлов перенесено в BridgeGames: " + n, Toast.LENGTH_LONG).show();
            });
        }, "bridge-storage").start();
    }

    /**
     * Starts the game with the saved values. CLEAR_TASK destroys a MainActivity that is still alive, so its
     * GameProcess (which reads settings.txt when it is created) is rebuilt from scratch.
     */
    private void restartGame() {
        startActivity(new Intent(this, MainActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK));
        finish();
    }

    // ------------------------------------------------------------ widgets

    private int px(int v) { return Math.round(v * dp); }

    private TextView text(String s, int sp, int color, boolean bold) {
        TextView t = new TextView(this);
        t.setText(s);
        t.setTextColor(color);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        if (bold) t.setTypeface(t.getTypeface(), android.graphics.Typeface.BOLD);
        return t;
    }

    private View spacer(int h) {
        View v = new View(this);
        v.setLayoutParams(new LinearLayout.LayoutParams(1, px(h)));
        return v;
    }

    private View section(String title) {
        LinearLayout l = new LinearLayout(this);
        l.setOrientation(LinearLayout.VERTICAL);
        l.setPadding(0, px(22), 0, px(4));
        l.addView(text(title.toUpperCase(), 12, ACCENT, true));
        return l;
    }

    private Button button(String label) {
        Button b = new Button(this);
        b.setText(label);
        b.setAllCaps(false);
        return b;
    }

    private int intOf(String key, int def) {
        try { return Integer.parseInt(values.getOrDefault(key, String.valueOf(def))); } catch (NumberFormatException e) { return def; }
    }

    private View slider(String title, String key, int min, int max, int step, int def, String unit, String hint) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.VERTICAL);
        row.setPadding(0, px(10), 0, px(4));
        TextView head = text("", 16, FG, false);
        SeekBar bar = new SeekBar(this);
        bar.setMax((max - min) / step);
        int cur = Math.max(min, Math.min(max, intOf(key, def)));
        bar.setProgress((cur - min) / step);
        Runnable show = () -> head.setText(title + ": " + (min + bar.getProgress() * step) + unit);
        show.run();
        bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar s, int p, boolean user) {
                show.run();
                values.put(key, String.valueOf(min + p * step));
            }
            @Override public void onStartTrackingTouch(SeekBar s) { }
            @Override public void onStopTrackingTouch(SeekBar s) { }
        });
        row.addView(head);
        row.addView(bar);
        row.addView(text(hint, 12, DIM, false));
        return row;
    }

    private View choice(String title, String key, int[] options, int def, String unit, String hint) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.VERTICAL);
        row.setPadding(0, px(10), 0, px(4));
        row.addView(text(title, 16, FG, false));
        LinearLayout line = new LinearLayout(this);
        line.setOrientation(LinearLayout.HORIZONTAL);
        line.setGravity(Gravity.START);
        int cur = intOf(key, def);
        Button[] bs = new Button[options.length];
        for (int i = 0; i < options.length; i++) {
            final int o = options[i];
            Button b = new Button(this);
            b.setAllCaps(false);
            b.setText(o + unit);
            b.setMinWidth(0);
            b.setMinimumWidth(0);
            bs[i] = b;
            b.setOnClickListener(v -> {
                values.put(key, String.valueOf(o));
                for (int j = 0; j < bs.length; j++) bs[j].setAlpha(options[j] == o ? 1f : 0.45f);
            });
            b.setAlpha(o == cur ? 1f : 0.45f);
            line.addView(b, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        }
        row.addView(line);
        row.addView(text(hint, 12, DIM, false));
        return row;
    }

    /** A whole number typed by hand; empty means "not set" and removes the key. */
    private View amount(String title, String key, int max, String hint) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.VERTICAL);
        row.setPadding(0, px(10), 0, px(4));
        row.addView(text(title, 16, FG, false));
        EditText in = new EditText(this);
        in.setInputType(InputType.TYPE_CLASS_NUMBER);
        in.setFilters(new InputFilter[] { new InputFilter.LengthFilter(String.valueOf(max).length()) });
        in.setTextColor(FG);
        in.setHintTextColor(DIM);
        in.setHint("не задано");
        in.setText(values.getOrDefault(key, ""));
        in.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int a, int b, int c) { }
            @Override public void onTextChanged(CharSequence s, int a, int b, int c) { }
            @Override public void afterTextChanged(Editable e) {
                String t = e.toString().trim();
                if (t.isEmpty()) { values.remove(key); return; }
                try { values.put(key, String.valueOf(Math.min(max, Long.parseLong(t)))); } catch (NumberFormatException x) { values.remove(key); }
            }
        });
        row.addView(in);
        row.addView(text(hint, 12, DIM, false));
        return row;
    }

    private View toggle(String title, String key, boolean def, String hint) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.VERTICAL);
        row.setPadding(0, px(10), 0, px(4));
        Switch sw = new Switch(this);
        sw.setText(title);
        sw.setTextColor(FG);
        sw.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        sw.setChecked(intOf(key, def ? 1 : 0) != 0);
        sw.setOnCheckedChangeListener((b, on) -> values.put(key, on ? "1" : "0"));
        row.addView(sw);
        row.addView(text(hint, 12, DIM, false));
        return row;
    }
}
