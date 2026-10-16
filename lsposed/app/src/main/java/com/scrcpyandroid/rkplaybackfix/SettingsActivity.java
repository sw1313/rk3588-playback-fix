package com.scrcpyandroid.rkplaybackfix;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.os.Bundle;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.util.Locale;

public final class SettingsActivity extends Activity {
    @Override protected void attachBaseContext(Context base) {
        String language = base.getSharedPreferences("interface", Context.MODE_PRIVATE).getString("language", "");
        if ("zh".equals(language) || "en".equals(language)) {
            Configuration configuration = new Configuration(base.getResources().getConfiguration());
            configuration.setLocale(Locale.forLanguageTag(language));
            base = base.createConfigurationContext(configuration);
        }
        super.attachBaseContext(base);
    }
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
        if (!preferences.contains("cropfix"))
            preferences.edit().putBoolean("cropfix", false).commit();
        LinearLayout column = new LinearLayout(this);
        column.setOrientation(LinearLayout.VERTICAL);
        int pad = (int) (32 * getResources().getDisplayMetrics().density);
        column.setPadding(pad, pad, pad, pad);
        TextView title = new TextView(this);
        title.setText(getString(R.string.settings_title, BuildConfig.VERSION_NAME)); title.setTextSize(24); column.addView(title);
        Button language = new Button(this);
        language.setText(R.string.language_button);
        language.setContentDescription(getString(R.string.language_description));
        language.setOnClickListener(button -> {
            boolean chinese = getResources().getConfiguration().getLocales().get(0).getLanguage().equals("zh");
            getSharedPreferences("interface", Context.MODE_PRIVATE).edit().putString("language", chinese ? "en" : "zh").commit();
            recreate();
        });
        column.addView(language);
        TextView description = new TextView(this);
        description.setText(getString(R.string.settings_description, getString(shared ? R.string.shared_available : R.string.shared_unavailable)));
        description.setTextSize(18); column.addView(description);
        addSwitch(column, preferences, "yamby_menu", getString(R.string.yamby_menu_label));
        addSwitch(column, preferences, "noncopy", getString(R.string.noncopy_label));
        addSwitch(column, preferences, "cropfix", getString(R.string.cropfix_label));
        TextView limits = new TextView(this);
        limits.setText(R.string.settings_limits);
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
