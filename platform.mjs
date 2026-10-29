/** Capability checks affect scheduling and export, never asset quality. */
export function executionProfile({cores = 2, memoryGiB = 0, mobile = false} = {}) {
  const limited = memoryGiB > 0 && memoryGiB <= 4;
  const textureWorkers = Math.max(1, Math.min(mobile || limited ? 2 : 4, Math.floor(cores / 2) || 1));
  return {textureWorkers, mobile, memoryGiB, cores};
}
export function browserProfile() {
  return executionProfile({cores: navigator.hardwareConcurrency || 2,
    memoryGiB: navigator.deviceMemory || 0,
    mobile: navigator.userAgentData?.mobile ?? matchMedia('(pointer: coarse)').matches});
}
export async function keepScreenAwake() {
  try { return await navigator.wakeLock?.request('screen'); } catch { return null; }
}
