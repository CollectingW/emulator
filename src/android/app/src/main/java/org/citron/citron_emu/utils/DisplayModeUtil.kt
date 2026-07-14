// SPDX-FileCopyrightText: 2026 Citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.citron.citron_emu.utils

import android.app.Activity

object DisplayModeUtil {
    fun preferHighestRefreshRate(activity: Activity) {
        val highestRefreshMode = activity.display?.supportedModes?.maxByOrNull { it.refreshRate }
            ?: return
        val layoutParams = activity.window.attributes
        layoutParams.preferredDisplayModeId = highestRefreshMode.modeId
        activity.window.attributes = layoutParams
    }
}
