// SPDX-License-Identifier: MIT
package com.scrcpyandroid.displaypolicy;

/** Read-only factory Binder query. Does not invoke a setter, player or UI. */
public final class ReadHdrState {
    public static void main(String[] args) throws Exception {
        Class<?> manager = Class.forName("android.os.RkDisplayOutputManager");
        Object service = manager.getConstructor().newInstance();
        Object enabled = manager.getMethod("isHDR10Status").invoke(service);
        if (!(enabled instanceof Boolean)) throw new IllegalStateException("Unexpected HDR query result");
        Class<?> props = Class.forName("android.os.SystemProperties");
        String live = (String) props.getMethod("get", String.class, String.class)
            .invoke(null, "vendor.hwc.hdr_force_disable", "");
        String saved = (String) props.getMethod("get", String.class, String.class)
            .invoke(null, "persist.vendor.rkdisplay.hdr_force_disable", "");
        System.out.println("FACTORY_HDR_MENU_ENABLED=" + enabled);
        System.out.println("RAW_HDR_DISABLE=" + live);
        System.out.println("SAVED_HDR_DISABLE=" + saved);
        int iface = (Integer) manager.getMethod("getCurrentInterface", int.class).invoke(service, 0);
        Object selected = manager.getMethod("getCurrentMode", int.class, int.class).invoke(service, 0, iface);
        Object stored = props.getMethod("get", String.class, String.class)
            .invoke(null, "persist.vendor.resolution.HDMI-A-0", "");
        System.out.println("FACTORY_PRIMARY_INTERFACE=" + iface);
        System.out.println("FACTORY_HDMI_MENU_SELECTION=" + selected);
        System.out.println("STORED_HDMI_SELECTION=" + stored);
    }
}
