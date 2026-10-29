/** Language follows the selected PC assets, independently of browser/console locale. */
export const LANGUAGES = Object.freeze([
  Object.freeze({code: 'en', name: 'English', archive: 'dialog.rcf', track: 0, bible: 'E'}),
  Object.freeze({code: 'fr', name: 'Français', archive: 'dialogf.rcf', track: 1, bible: 'F'}),
  Object.freeze({code: 'es', name: 'Español', archive: 'dialogs.rcf', track: 3, bible: 'S'}),
]);

export function languageProfile(code) {
  const profile = LANGUAGES.find(item => item.code === code);
  if (!profile) throw new Error('Choose English, French or Spanish from your PC copy.');
  return profile;
}

export function availableLanguages(files) {
  const names = new Set([...files.keys()].map(path => path.toLowerCase()));
  return LANGUAGES.filter(item => names.has(item.archive));
}

export function selectLanguage(files, requested) {
  const available = availableLanguages(files);
  if (!available.length) throw new Error('The PC copy is missing its English, French or Spanish dialogue archive.');
  const selected = requested ? languageProfile(requested) : available[0];
  if (!available.includes(selected)) throw new Error(`This copy does not include ${selected.name} dialogue (${selected.archive}).`);
  return selected;
}
