// Start native UI only after the selecting touch/command and queued redraw finish.
// The caller supplies OTA_update and InkView's SetHardTimer.
static const char *otaPendingBranch = NULL;
static void OTA_runScheduledUpdate() {
    const char *branch = otaPendingBranch;
    otaPendingBranch = NULL;
    if (branch) OTA_update(branch);
}
void OTA_scheduleUpdate(const char *branch) {
    if (otaPendingBranch) return;
    // Retain static channel names, never a pointer owned by the caller.
    otaPendingBranch = !strcmp(branch, "master") ? "master" : "dev";
    SetHardTimer("OTA_start", OTA_runScheduledUpdate, 150);
}
