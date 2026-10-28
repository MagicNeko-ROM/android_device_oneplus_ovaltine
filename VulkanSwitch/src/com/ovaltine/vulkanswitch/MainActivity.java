/*
 * SPDX-FileCopyrightText: 2026 The MagicNekoROM Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.ovaltine.vulkanswitch;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Front end for /data/misc/vulkan/per_app.conf, the file vulkan.switcher.so
 * reads inside every process that creates a Vulkan instance.
 *
 * Line format is "&lt;package&gt;=&lt;subname&gt;", "*" being the catch-all default.
 * The shim reads the file once per instance creation, so a change only takes
 * effect after the target app is force-stopped.
 *
 * This ships as a privileged vendor app running in the system UID, which is
 * what makes the config file writable at all: /data/misc/vulkan is owned by
 * system and SELinux only grants write on vulkan_perapp_conf_file to
 * system_app. The file is left world-readable on purpose, because the shim
 * reads it inside the *client* process (untrusted_app), not here.
 */
public class MainActivity extends Activity {
    private static final String CONF_DIR = "/data/misc/vulkan";
    private static final String CONF_PATH = CONF_DIR + "/per_app.conf";
    private static final String GLOBAL = "*";
    private static final String[] DRIVERS = {"adreno", "freedreno"};

    private final Map<String, String> mConf = new LinkedHashMap<>();
    private final List<AppEntry> mApps = new ArrayList<>();

    private TextView mStatus;
    private Button mGlobalButton;
    private BaseAdapter mAdapter;

    private static final class AppEntry {
        String pkg;
        String label;
        AppEntry(String pkg, String label) { this.pkg = pkg; this.label = label; }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);

        final int pad = (int) (16 * getResources().getDisplayMetrics().density);

        LinearLayout header = new LinearLayout(this);
        header.setOrientation(LinearLayout.VERTICAL);
        header.setPadding(pad, pad, pad, pad / 2);

        TextView title = new TextView(this);
        title.setText("Vulkan 驱动切换");
        title.setTextSize(20);
        header.addView(title);

        mStatus = new TextView(this);
        mStatus.setTextSize(12);
        mStatus.setPadding(0, pad / 2, 0, pad / 2);
        header.addView(mStatus);

        mGlobalButton = new Button(this);
        mGlobalButton.setOnClickListener(v -> pickDriver(GLOBAL));
        header.addView(mGlobalButton);

        Button reset = new Button(this);
        reset.setText("清空全部配置");
        reset.setOnClickListener(v -> {
            mConf.clear();
            if (saveConf()) {
                rebuild();
                toast("已清空，所有应用回到默认驱动");
            }
        });
        header.addView(reset);

        TextView hint = new TextView(this);
        hint.setTextSize(12);
        hint.setText("下面是已安装的应用。点一下选驱动；改完要强制停止目标应用才生效。");
        hint.setPadding(0, pad / 2, 0, pad / 2);
        header.addView(hint);

        root.addView(header);

        ListView list = new ListView(this);
        mAdapter = new AppAdapter();
        list.setAdapter(mAdapter);
        list.setOnItemClickListener((parent, view, position, id) -> {
            if (position >= 0 && position < mApps.size()) {
                pickDriver(mApps.get(position).pkg);
            }
        });
        root.addView(list, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        setContentView(root);

        loadConf();
        loadApps();
        rebuild();
    }

    @Override
    protected void onResume() {
        super.onResume();
        loadConf();
        rebuild();
    }

    /** Parses the conf, dropping comments and anything unparsable. */
    private void loadConf() {
        mConf.clear();
        File f = new File(CONF_PATH);
        if (!f.exists()) {
            return;
        }
        try (BufferedReader r = new BufferedReader(new FileReader(f))) {
            String line;
            while ((line = r.readLine()) != null) {
                int hash = line.indexOf('#');
                if (hash >= 0) {
                    line = line.substring(0, hash);
                }
                line = line.trim();
                if (line.isEmpty()) {
                    continue;
                }
                int eq = line.indexOf('=');
                if (eq <= 0) {
                    continue;
                }
                String key = line.substring(0, eq).trim();
                String value = line.substring(eq + 1).trim();
                if (!key.isEmpty() && !value.isEmpty()) {
                    mConf.put(key, value);
                }
            }
        } catch (IOException ignored) {
            // An unreadable conf behaves like an empty one: the shim falls back.
        }
    }

    /**
     * Creates the directory if init did not, then rewrites the whole file.
     * The file is made world-readable because every app parses it through the
     * shim, and we are the only uid allowed to write it.
     */
    private boolean saveConf() {
        File dir = new File(CONF_DIR);
        if (!dir.exists() && !dir.mkdirs()) {
            toast("无法创建 " + CONF_DIR);
            return false;
        }
        File f = new File(CONF_PATH);
        StringBuilder sb = new StringBuilder();
        String global = mConf.get(GLOBAL);
        if (global != null) {
            sb.append(GLOBAL).append('=').append(global).append('\n');
        }
        List<String> keys = new ArrayList<>(mConf.keySet());
        Collections.sort(keys);
        for (String k : keys) {
            if (!k.equals(GLOBAL)) {
                sb.append(k).append('=').append(mConf.get(k)).append('\n');
            }
        }
        try (FileWriter w = new FileWriter(f, false)) {
            w.write(sb.toString());
            w.flush();
        } catch (IOException e) {
            toast("写入失败: " + e.getMessage());
            return false;
        }
        if (!f.setReadable(true, false)) {
            toast("警告：设置 0644 失败，其他应用可能读不到");
        }
        return true;
    }

    /** Launchable apps, sorted by label, one row per package. */
    private void loadApps() {
        mApps.clear();
        PackageManager pm = getPackageManager();
        List<ResolveInfo> resolved = pm.queryIntentActivities(
                new android.content.Intent(android.content.Intent.ACTION_MAIN)
                        .addCategory(android.content.Intent.CATEGORY_LAUNCHER), 0);
        java.util.HashSet<String> seen = new java.util.HashSet<>();
        for (ResolveInfo ri : resolved) {
            String pkg = ri.activityInfo.packageName;
            if (pkg == null || !seen.add(pkg)) {
                continue;
            }
            AppEntry e = new AppEntry(pkg, ri.loadLabel(pm).toString());
            mApps.add(e);
        }
        Collections.sort(mApps, new Comparator<AppEntry>() {
            @Override
            public int compare(AppEntry a, AppEntry b) {
                return a.label.compareToIgnoreCase(b.label);
            }
        });
    }

    private void rebuild() {
        String global = mConf.get(GLOBAL);
        if (global == null) {
            global = "未设置（默认 adreno）";
        }
        mStatus.setText("全局默认：" + global + "\n配置文件：" + CONF_PATH
                + "\n当前配置 " + mConf.size() + " 条");
        mGlobalButton.setText("全局默认：" + global + "（点击修改）");
        mAdapter.notifyDataSetChanged();
    }

    private void pickDriver(final String pkg) {
        final String[] items = {"adreno（高通原厂）", "freedreno（Mesa Turnip）", "跟随全局默认", "取消"};
        final String current = mConf.get(pkg);
        new AlertDialog.Builder(this)
                .setTitle(GLOBAL.equals(pkg) ? "全局默认驱动"
                        : pkg + "\n当前：" + (current == null ? "跟随全局" : current))
                .setItems(items, (d, which) -> {
                    if (which == 0 || which == 1) {
                        mConf.put(pkg, DRIVERS[which]);
                    } else if (which == 2) {
                        if (GLOBAL.equals(pkg)) {
                            mConf.remove(GLOBAL);
                        } else {
                            mConf.remove(pkg);
                        }
                    } else {
                        return;
                    }
                    if (saveConf()) {
                        rebuild();
                        toast(GLOBAL.equals(pkg)
                                ? "全局默认已更新"
                                : pkg + " 已设置，记得强制停止它");
                    }
                })
                .show();
    }

    private String describe(String pkg) {
        String own = mConf.get(pkg);
        if (!TextUtils.isEmpty(own)) {
            return own;
        }
        String global = mConf.get(GLOBAL);
        return global == null ? "adreno（默认）" : global + "（全局）";
    }

    /**
     * Two-line rows built by hand. android.R.layout.simple_list_item_2 is a
     * TwoLineListItem, not a TextView, so the ArrayAdapter constructor rejects
     * it at runtime ("ArrayAdapter requires the resource ID to be a TextView").
     */
    private final class AppAdapter extends BaseAdapter {
        @Override
        public int getCount() {
            return mApps.size();
        }

        @Override
        public Object getItem(int position) {
            return mApps.get(position);
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            LinearLayout row;
            if (convertView instanceof LinearLayout) {
                row = (LinearLayout) convertView;
            } else {
                float density = getResources().getDisplayMetrics().density;
                int pad = (int) (12 * density);
                row = new LinearLayout(MainActivity.this);
                row.setOrientation(LinearLayout.VERTICAL);
                row.setPadding(pad, pad / 2, pad, pad / 2);

                TextView label = new TextView(MainActivity.this);
                label.setTextSize(16);
                label.setId(android.R.id.text1);
                row.addView(label);

                TextView detail = new TextView(MainActivity.this);
                detail.setTextSize(12);
                detail.setId(android.R.id.text2);
                row.addView(detail);
            }

            AppEntry e = mApps.get(position);
            ((TextView) row.findViewById(android.R.id.text1)).setText(e.label);
            ((TextView) row.findViewById(android.R.id.text2))
                    .setText(e.pkg + "  \u2192  " + describe(e.pkg));
            return row;
        }
    }

    private void toast(String msg) {
        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
    }
}
