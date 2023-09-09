#!/bin/bash
#
# Copyright (C) 2016 The CyanogenMod Project
# Copyright (C) 2017-2020 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

function blob_fixup() {
    case "${1}" in
        odm/overlayfs/*/lib/libmmsw_platform.so|odm/overlayfs/*/lib/libmmsw_detail_enhancement.so)
            "${PATCHELF}" --remove-needed "libbinder.so" "${2}"
            sed -i 's|libgui.so|libwui.so|g' "${2}"
            ;;
        odm/overlayfs/*/lib/libmmcamera2_sensor_modules.so)
            sed -i 's|/system/etc/camera/|////odm/etc/camera/|g' "${2}"
            sed -i 's|data/misc/camera|data/vendor/qcam|g' "${2}"
            ;;
        odm/overlayfs/*/lib/libmmcamera_tintless_bg_pca_algo.so \
        |odm/overlayfs/*/lib/libmmcamera_pdafcamif.so \
        |odm/overlayfs/*/lib/libmmcamera2_dcrf.so \
        |odm/overlayfs/*/lib/libmmcamera_imglib.so \
        |odm/overlayfs/*/lib/libmmcamera_dbg.so \
        |odm/overlayfs/*/lib/libmmcamera2_stats_algorithm.so \
        |odm/overlayfs/*/lib/libmmcamera2_mct.so \
        |odm/overlayfs/*/lib/libmmcamera_tuning.so \
        |odm/overlayfs/*/lib/libmmcamera_tintless_algo.so \
        |odm/overlayfs/*/lib/libmmcamera2_iface_modules.so \
        |odm/overlayfs/*/lib/libmmcamera2_q3a_core.so \
        |odm/overlayfs/*/lib/libmmcamera2_pproc_modules.so \
        |odm/overlayfs/*/lib/libmmcamera2_imglib_modules.so \
        |odm/overlayfs/*/lib/libmmcamera2_cpp_module.so \
        |odm/overlayfs/*/lib/libmmcamera_pdaf.so \
        |odm/overlayfs/*/bin/mm-qcamera-daemon)
            sed -i 's|data/misc/camera|data/vendor/qcam|g' "${2}"
            ;;
        odm/overlayfs/*/lib/libmmcamera2_stats_modules.so)
            sed -i 's|data/misc/camera|data/vendor/qcam|g' "${2}"
            sed -i 's|libgui.so|libwui.so|g' "${2}"
            "${PATCHELF}" --replace-needed "libandroid.so" "libshims_android.so" "${2}"
            ;;
        odm/overlayfs/*/lib/libmmcamera_ppeiscore.so)
            sed -i 's|libgui.so|libwui.so|g' "${2}"
            if ! "${PATCHELF}" --print-needed "${2}" | grep "libshims_ui.so" >/dev/null; then
                "${PATCHELF}" --add-needed "libshims_ui.so" "${2}"
            fi
            ;;
        odm/overlayfs/*/lib/libmpbase.so)
            "${PATCHELF}" --replace-needed "libandroid.so" "libshims_android.so" "${2}"
            ;;
    esac
}

# If we're being sourced by the common script that we called,
# stop right here. No need to go down the rabbit hole.
if [ "${BASH_SOURCE[0]}" != "${0}" ]; then
    return
fi

set -e

export DEVICE=Mi8917
export DEVICE_COMMON=mithorium-common
export VENDOR=xiaomi

"./../../${VENDOR}/${DEVICE_COMMON}/extract-files.sh" "$@"
