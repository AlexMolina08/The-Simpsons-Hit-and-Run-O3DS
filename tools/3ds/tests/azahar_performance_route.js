// Run through the documented CUA native-app API after gameplay is visible.
// These fixed-duration inputs are repeatable; simulation/traffic are not a
// deterministic replay. Start from the same save and camera for comparisons.
async function azaharPerformanceRoute(app) {
    await app.pressKey('Up', {durationMs: 2500});
    await app.pressKey('Left', {durationMs: 1200});
    await app.pressKey('z', {durationMs: 300});
    await app.getAXState();
}
