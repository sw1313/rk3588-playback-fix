package com.scrcpyandroid.rkplaybackfix;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

public final class SettingsActivity extends Activity {
    @Override @SuppressWarnings("deprecation") public void onCreate(Bundle saved) {
        super.onCreate(saved);
        SharedPreferences preferences;
        boolean shared;
        try {
            preferences = getSharedPreferences("fixes", Context.MODE_WORLD_READABLE);
            shared = true;
        } catch (SecurityException e) {
            preferences = getSharedPreferences("fixes", Context.MODE_PRIVATE);
            shared = false;
        }
        if (!preferences.contains("noncopy"))
            preferences.edit().putBoolean("noncopy", true).commit();
        if (!preferences.contains("yamby_menu"))
            preferences.edit().putBoolean("yamby_menu", true).commit();
        LinearLayout column = new LinearLayout(this);
        column.setOrientation(LinearLayout.VERTICAL);
        int pad = (int) (32 * getResources().getDisplayMetrics().density);
        column.setPadding(pad, pad, pad, pad);
        TextView title = new TextView(this);
        title.setText("RK3588 mpv 兼容修复 · 0.3.0"); title.setTextSize(24); column.addView(title);
        TextView description = new TextView(this);
        description.setText("在 LSPosed 中勾选需要修复的 mpv 播放器，然后重新启动对应应用。\n"
            + "Yamby 在自己的“默认解码方式”中选择 HW+、HW 或 SW，播放时仍可手动切换。\n"
            + (shared ? "LSPosed 共享设置可用。" : "LSPosed 尚未接管设置：启用模块后重新打开本页。"));
        description.setTextSize(18); column.addView(description);
        addSwitch(column, preferences, "yamby_menu", "为 Yamby 默认解码设置补上 HW+");
        addSwitch(column, preferences, "noncopy", "其他 mpv 播放器：硬解优先使用 HW+（保留 SW）");
        TextView limits = new TextView(this);
        limits.setText("作用域由你选择，默认仅推荐已验证的 Yamby。本模块自动识别常见 mpv JNI 接口；\n"
            + "没有匹配接口时仅记录诊断信息。纯 native 调用或完全混淆的封装可能需要另行适配。\n"
            + "gpu-next 设置由原播放器控制。HDR 色彩、字幕和拖动仍需实测。日志标签：RKPlaybackFix。");
        limits.setTextSize(16); column.addView(limits);
        ScrollView scroll = new ScrollView(this); scroll.addView(column); setContentView(scroll);
    }
    private void addSwitch(LinearLayout column, SharedPreferences preferences, String key, String label) {
        CheckBox box = new CheckBox(this); box.setText(label); box.setTextSize(20);
        box.setChecked(preferences.getBoolean(key, true));
        box.setOnCheckedChangeListener((button, checked) -> preferences.edit().putBoolean(key, checked).commit());
        column.addView(box);
    }
}
