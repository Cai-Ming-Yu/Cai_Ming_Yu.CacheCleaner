[ "${API}" -lt '21' ] && {
    ui_print "! API ${API} is too old"
    ui_print "- Cache Cleaner is not supported on this Android version"
    abort
}

{ [ "${BOOTMODE}" == 'true' ] && [ "$(pm 'path' 'Cai_Ming_Yu.CacheCleaner')" == '' ]; } && {
    ui_print "! Cache Cleaner xposed version is not installed."
    ui_print "- Please install it for a better experience."
}

verifyList="$(unzip '-p' "${ZIPFILE}" "files.conf")"

verifyFile() {
    local trueSum="$(echo -n "${verifyList}" | grep " ${1}" | awk '{print $1}')"
    unzip '-oj' "${ZIPFILE}" "${1}" '-d' "${MODPATH}/${2}" 1>&'2'
    [ "${trueSum}" != "$(md5sum '-b' "${MODPATH}/${2}/$(basename "${1}")" | cut -d' ' '-f1')" ] && {
        ui_print "! Failed to extract the file: ${1}"
        ui_print "- Please go to https://github.com/Cai-Ming-Yu/CMY-CacheCleaner to re-download the module!"
        abort
    } || {
        ui_print "- Extracted and verified the file: ${1}"
    }
}

verifyFile 'module.prop'
verifyFile 'service.sh'
verifyFile 'uninstall.sh'
verifyFile "bin/${ARCH}/CacheCleaner"

case "$(getprop 'persist.sys.locale')" in
    'zh-CN'|'zh-SG') verifyFile "config_zh-CN.yaml" ; mv "${MODPATH}/config_zh-CN.yaml" "${MODPATH}/config.yaml" ;;
    *) verifyFile 'config.yaml' ;;
esac

[ ! -d "/data/adb/C-M-Y/CacheCleaner" ] && mkdir '-p' "/data/adb/C-M-Y/CacheCleaner"
[ ! -f "/data/adb/C-M-Y/CacheCleaner/config.yaml" ] && {
    cp '-rf' "${MODPATH}/config.yaml" "/data/adb/C-M-Y/CacheCleaner/config.yaml"
}

[ "$(pidof 'CacheCleaner')" != '' ] && {
    sh "${MODPATH}/service.sh" &
    ui_print "- Cache Cleaner has been restarted"
} || {
    ui_print "- Cache Cleaner will run after reboot"
}

ui_print "- configuration file: /data/adb/C-M-Y/CacheCleaner/config.yaml"
ui_print "- Installation is complete, thank you for using"
