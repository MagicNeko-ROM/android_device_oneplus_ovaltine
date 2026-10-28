#!/system/bin/sh
# Runtime Vulkan ICD selector for the hwvulkan switcher.
#   vulkan-switch get
#   vulkan-switch set <package|*> <adreno|freedreno>
CONF=/data/misc/vulkan/per_app.conf
case "$1" in
  get) cat "$CONF" 2>/dev/null || echo "(no config)" ;;
  list) echo "adreno (OEM Qualcomm) / freedreno (Mesa Turnip)" ;;
  set)
    pkg="$2"; drv="$3"
    case "$drv" in adreno|freedreno) ;; *) echo "driver must be adreno or freedreno"; exit 2;; esac
    [ -z "$pkg" ] && { echo "usage: vulkan-switch set <package|*> <adreno|freedreno>"; exit 2; }
    mkdir -p /data/misc/vulkan
    if [ -f "$CONF" ]; then grep -v "^${pkg}=" "$CONF" > "$CONF.tmp" 2>/dev/null; mv "$CONF.tmp" "$CONF"; fi
    echo "${pkg}=${drv}" >> "$CONF"
    echo "set ${pkg} -> ${drv}; restart the app to apply"
    ;;
  *) echo "usage: vulkan-switch [get|list|set <package|*> <adreno|freedreno>]"; exit 1 ;;
esac
