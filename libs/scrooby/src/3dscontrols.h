#ifndef SCROOBY_3DS_CONTROLS_H
#define SCROOBY_3DS_CONTROLS_H

#include <stddef.h>
#include <string.h>

// Applied once while loading a text bible, never during the render loop.
namespace Scrooby3DSControls
{
struct Entry { const char* key; const char* text; };
static const Entry entries[] = {
    {"TUTORIAL_000", "TO DESTROY WASP CAMERAS, JUMP WITH A, THEN PRESS Y TO KICK."},
    {"TUTORIAL_002", "MOVE WITH THE CIRCLE PAD.\\PRESS A TO JUMP.\\HOLD B TO RUN.\\HOLD L + CIRCLE PAD TO MOVE THE CAMERA."},
    {"TUTORIAL_002_(PS2)", "MOVE WITH THE CIRCLE PAD.\\PRESS A TO JUMP.\\HOLD B TO RUN."},
    {"TUTORIAL_003", "PRESS A TO ACCELERATE.\\STEER WITH THE CIRCLE PAD.\\PRESS B TO BRAKE OR REVERSE.\\PRESS Y OR R TO HANDBRAKE."},
    {"TUTORIAL_003_(GC)", "A: ACCELERATE. CIRCLE PAD: STEER. B: BRAKE / REVERSE. Y OR R: HANDBRAKE."},
    {"TUTORIAL_003_(PS2)", "A: ACCELERATE. CIRCLE PAD: STEER. B: BRAKE / REVERSE. Y OR R: HANDBRAKE."},
    {"TUTORIAL_004", "PRESS X TO ENTER OR EXIT A CAR."},
    {"TUTORIAL_009", "PRESS X TO USE THE PHONE BOOTH."},
    {"TUTORIAL_010", "PRESS X TO ENTER OR EXIT A CAR."},
    {"TUTORIAL_011", "PRESS Y TO KICK THE BOX."},
    {"TUTORIAL_013", "PRESS X TO GO INSIDE."},
    {"TUTORIAL_018", "PRESS X TO ENTER OR EXIT A CAR."},
    {"PRESS_START", "PRESS START"},
};
static const Entry frenchEntries[] = {
    {"TUTORIAL_000", "POUR DÉTRUIRE UNE GUÊPE CAMÉRA, SAUTE AVEC A, PUIS FRAPPE AVEC Y."},
    {"TUTORIAL_002", "DÉPLACE-TOI AVEC LE PAD CIRCULAIRE.\\A : SAUTER. B : COURIR.\\L + PAD CIRCULAIRE : CAMÉRA."},
    {"TUTORIAL_002_(PS2)", "PAD CIRCULAIRE : SE DÉPLACER.\\A : SAUTER. B : COURIR."},
    {"TUTORIAL_003", "A : ACCÉLÉRER.\\PAD CIRCULAIRE : TOURNER.\\B : FREINER OU RECULER.\\Y OU R : FREIN DE PARKING."},
    {"TUTORIAL_003_(GC)", "A : ACCÉLÉRER. PAD : TOURNER. B : FREINER OU RECULER. Y OU R : FREIN DE PARKING."},
    {"TUTORIAL_003_(PS2)", "A : ACCÉLÉRER. PAD : TOURNER. B : FREINER OU RECULER. Y OU R : FREIN DE PARKING."},
    {"TUTORIAL_004", "APPUIE SUR X POUR ENTRER DANS UN VÉHICULE OU EN SORTIR."},
    {"TUTORIAL_009", "APPUIE SUR X POUR UTILISER LA CABINE TÉLÉPHONIQUE."},
    {"TUTORIAL_010", "APPUIE SUR X POUR ENTRER DANS UN VÉHICULE OU EN SORTIR."},
    {"TUTORIAL_011", "APPUIE SUR Y POUR FRAPPER LA CAISSE."},
    {"TUTORIAL_013", "APPUIE SUR X POUR ENTRER."},
    {"TUTORIAL_018", "APPUIE SUR X POUR ENTRER DANS UN VÉHICULE OU EN SORTIR."},
    {"PRESS_START", "APPUIE SUR START"},
};
static const Entry spanishEntries[] = {
    {"TUTORIAL_000", "PARA DESTRUIR UNA CÁMARA AVISPA, SALTA CON A Y GOLPEA CON Y."},
    {"TUTORIAL_002", "MUEVE CON EL BOTÓN DESLIZANTE.\\A: SALTAR. B: CORRER.\\L + BOTÓN DESLIZANTE: CÁMARA."},
    {"TUTORIAL_002_(PS2)", "BOTÓN DESLIZANTE: MOVERSE.\\A: SALTAR. B: CORRER."},
    {"TUTORIAL_003", "A: ACELERAR.\\BOTÓN DESLIZANTE: GIRAR.\\B: FRENAR O DAR MARCHA ATRÁS.\\Y O R: FRENO DE MANO."},
    {"TUTORIAL_003_(GC)", "A: ACELERAR. BOTÓN DESLIZANTE: GIRAR. B: FRENAR O DAR MARCHA ATRÁS. Y O R: FRENO DE MANO."},
    {"TUTORIAL_003_(PS2)", "A: ACELERAR. BOTÓN DESLIZANTE: GIRAR. B: FRENAR O DAR MARCHA ATRÁS. Y O R: FRENO DE MANO."},
    {"TUTORIAL_004", "PULSA X PARA ENTRAR EN UN VEHÍCULO O SALIR DE ÉL."},
    {"TUTORIAL_009", "PULSA X PARA USAR LA CABINA TELEFÓNICA."},
    {"TUTORIAL_010", "PULSA X PARA ENTRAR EN UN VEHÍCULO O SALIR DE ÉL."},
    {"TUTORIAL_011", "PULSA Y PARA GOLPEAR LA CAJA."},
    {"TUTORIAL_013", "PULSA X PARA ENTRAR."},
    {"TUTORIAL_018", "PULSA X PARA ENTRAR EN UN VEHÍCULO O SALIR DE ÉL."},
    {"PRESS_START", "PULSA START"},
};
static inline unsigned short Next(const char*& text)
{
    const unsigned char* p = (const unsigned char*)text;
    unsigned short code = *p;
    unsigned count = 1;
    if((p[0]&0xe0)==0xc0 && p[1] && (p[1]&0xc0)==0x80) {
        code = ((p[0]&31)<<6) | (p[1]&63); count = 2;
    } else if((p[0]&0xf0)==0xe0 && p[1] && p[2] &&
              (p[1]&0xc0)==0x80 && (p[2]&0xc0)==0x80) {
        code = ((p[0]&15)<<12) | ((p[1]&63)<<6) | (p[2]&63); count = 3;
    }
    text += count;
    return code;
}
static inline unsigned Length(const char* text)
{
    unsigned words = 0;
    while(*text) { Next(text); ++words; }
    return words;
}
static inline unsigned Copy(unsigned short* output,const char* text)
{
    unsigned words = 0;
    while(*text) output[words++] = Next(text);
    return words;
}
static inline unsigned Hash(const char* key, unsigned modulo)
{
    unsigned value = 0;
    if(!modulo) return 0;
    while(*key) value = (64u * value + (unsigned char)*key++) % modulo;
    return value;
}
// Additional handheld UI labels are exact matches; retail narrative stays intact.
static constexpr unsigned CaptionHash(const char* text)
{
    unsigned value = 2166136261u;
    while(*text) value = (value ^ (unsigned char)*text++) * 16777619u;
    return value;
}
static inline const char* Caption(const char* original,char language)
{
    if(language != 'F' && language != 'S') return NULL;
    struct CaptionEntry {unsigned key; const char* english; const char* french; const char* spanish;};
    static const CaptionEntry captions[] = {
        {CaptionHash("GRAPHICS"), "GRAPHICS", "GRAPHISMES", "GRÁFICOS"},
        {CaptionHash("CHEATS"), "CHEATS", "CODES", "TRUCOS"},
        {CaptionHash("PHONE BOOTH"), "PHONE BOOTH", "CABINE TÉLÉPHONIQUE", "CABINA TELEFÓNICA"},
        {CaptionHash("SHOP"), "SHOP", "BOUTIQUE", "TIENDA"},
        {CaptionHash("LOADING"), "LOADING", "CHARGEMENT", "CARGANDO"},
        {CaptionHash("PREVIEW UNAVAILABLE"), "PREVIEW UNAVAILABLE", "APERÇU INDISPONIBLE", "VISTA NO DISPONIBLE"},
        {CaptionHash("THE END"), "THE END", "FIN", "FIN"},
        {CaptionHash("CREDITS"), "CREDITS", "CRÉDITS", "CRÉDITOS"},
        {CaptionHash("MAIN MENU"), "MAIN MENU", "MENU PRINCIPAL", "MENÚ PRINCIPAL"},
        {CaptionHash("BACK"), "BACK", "RETOUR", "ATRÁS"},
        {CaptionHash("A CONTINUE"), "A CONTINUE", "A CONTINUER", "A CONTINUAR"},
        {CaptionHash("A BACK"), "A BACK", "A RETOUR", "A ATRÁS"},
        {CaptionHash("B BACK"), "B BACK", "B RETOUR", "B ATRÁS"},
        {CaptionHash("A SELECT"), "A SELECT", "A CHOISIR", "A ELEGIR"},
        {CaptionHash("A Select"), "A Select", "A Choisir", "A Elegir"},
        {CaptionHash("B Back"), "B Back", "B Retour", "B Atrás"},
        {CaptionHash("CHANGE"), "CHANGE", "CHANGER", "CAMBIAR"},
        {CaptionHash("A Continue"), "A Continue", "A Continuer", "A Continuar"},
        {CaptionHash("X: Hide tutorials"), "X: Hide tutorials", "X : Masquer les conseils", "X: Ocultar tutoriales"},
        {CaptionHash("Vehicle:"), "Vehicle:", "Véhicule :", "Vehículo:"},
        {CaptionHash("Teleport:"), "Teleport:", "Destination :", "Destino:"},
        {CaptionHash("NO TIMERS"), "NO TIMERS", "SANS CHRONO", "SIN LÍMITE DE TIEMPO"},
        {CaptionHash("OFF"), "OFF", "NON", "NO"},
        {CaptionHash("ON"), "ON", "OUI", "SÍ"},
        {CaptionHash("CHEAT"), "CHEAT", "CODE", "TRUCO"},
        {CaptionHash("QUALITY MODE"), "QUALITY MODE", "QUALITÉ", "CALIDAD"},
        {CaptionHash("APPLY"), "APPLY", "APPLIQUER", "APLICAR"},
        {CaptionHash("UNLOCKED"), "UNLOCKED", "DÉBLOQUÉ", "DESBLOQUEADO"},
        {CaptionHash("TURN OFF"), "TURN OFF", "DÉSACTIVER", "DESACTIVAR"},
        {CaptionHash("TURN ON"), "TURN ON", "ACTIVER", "ACTIVAR"},
        {CaptionHash("UNLOCK ALL"), "UNLOCK ALL", "TOUT DÉBLOQUER", "DESBLOQUEAR TODO"},
        {CaptionHash("UNLOCK CARDS"), "UNLOCK CARDS", "DÉBLOQUER LES CARTES", "DESBLOQUEAR CROMOS"},
        {CaptionHash("UNLOCK OUTFITS"), "UNLOCK OUTFITS", "DÉBLOQUER LES TENUES", "DESBLOQUEAR TRAJES"},
        {CaptionHash("UNLOCK MISSIONS"), "UNLOCK MISSIONS", "DÉBLOQUER LES MISSIONS", "DESBLOQUEAR MISIONES"},
        {CaptionHash("UNLOCK MOVIES"), "UNLOCK MOVIES", "DÉBLOQUER LES VIDÉOS", "DESBLOQUEAR VÍDEOS"},
        {CaptionHash("UNLOCK CARS"), "UNLOCK CARS", "DÉBLOQUER LES VÉHICULES", "DESBLOQUEAR COCHES"},
        {CaptionHash("+100 COINS"), "+100 COINS", "+100 PIÈCES", "+100 MONEDAS"},
        {CaptionHash("INVINCIBLE CAR"), "INVINCIBLE CAR", "VÉHICULE INVINCIBLE", "COCHE INVENCIBLE"},
        {CaptionHash("NO SPEED LIMIT"), "NO SPEED LIMIT", "VITESSE ILLIMITÉE", "VELOCIDAD ILIMITADA"},
        {CaptionHash("FAST ACCELERATION"), "FAST ACCELERATION", "ACCÉLÉRATION RAPIDE", "ACELERACIÓN RÁPIDA"},
        {CaptionHash("HORN JUMP"), "HORN JUMP", "KLAXON POUR SAUTER", "SALTAR CON EL CLAXON"},
        {CaptionHash("ONE-HIT TRAFFIC"), "ONE-HIT TRAFFIC", "UN COUP SUFFIT", "TRÁFICO DE UN GOLPE"},
        {CaptionHash("WRECKED CAR LOOK"), "WRECKED CAR LOOK", "ASPECT ENDOMMAGÉ", "ASPECTO DESTROZADO"},
        {CaptionHash("RED BRICK CARS"), "RED BRICK CARS", "VÉHICULES EN BRIQUES", "COCHES DE LADRILLOS"},
        {CaptionHash("KICK SWAPS MODEL"), "KICK SWAPS MODEL", "FRAPPER CHANGE LE MODÈLE", "PATADA CAMBIA MODELO"},
        {CaptionHash("EXTRA CAMERAS"), "EXTRA CAMERAS", "CAMÉRAS SUPPLÉMENTAIRES", "CÁMARAS ADICIONALES"},
        {CaptionHash("SPEEDOMETER"), "SPEEDOMETER", "COMPTEUR DE VITESSE", "VELOCÍMETRO"},
        {CaptionHash("SHOW POSITION"), "SHOW POSITION", "AFFICHER LA POSITION", "MOSTRAR POSICIÓN"},
        {CaptionHash("CREDITS DIALOGUE"), "CREDITS DIALOGUE", "DIALOGUE DU GÉNÉRIQUE", "DIÁLOGO DE CRÉDITOS"},
        {CaptionHash("5X MISSION TIME"), "5X MISSION TIME", "TEMPS DE MISSION X5", "TIEMPO DE MISIÓN X5"},
        {CaptionHash("STANDARD DISPLAY"), "STANDARD DISPLAY", "AFFICHAGE STANDARD", "PANTALLA ESTÁNDAR"},
        {CaptionHash("SLIDER CONTROLS 3D DEPTH"), "SLIDER CONTROLS 3D DEPTH", "CURSEUR : PROFONDEUR 3D", "DESLIZADOR: PROFUNDIDAD 3D"},
        {CaptionHash("ON: RICHER LIGHTING"), "ON: RICHER LIGHTING", "OUI : ÉCLAIRAGE AMÉLIORÉ", "SÍ: ILUMINACIÓN MEJORADA"},
        {CaptionHash("ON: SHARPER IMAGE"), "ON: SHARPER IMAGE", "OUI : IMAGE PLUS NETTE", "SÍ: IMAGEN MÁS NÍTIDA"},
        {CaptionHash("QUALITY UNAVAILABLE"), "QUALITY UNAVAILABLE", "QUALITÉ INDISPONIBLE", "CALIDAD NO DISPONIBLE"},
        {CaptionHash("APPLYING..."), "APPLYING...", "APPLICATION...", "APLICANDO..."},
        {CaptionHash("400x240 PER EYE"), "400x240 PER EYE", "400x240 EN 3D", "400x240 POR OJO"},
        {CaptionHash("800x240 / 60 FPS TARGET"), "800x240 / 60 FPS TARGET", "800x240 / CIBLE 60 IPS", "800x240 / OBJETIVO 60 FPS"},
        {CaptionHash("800x240 / 30 FPS TARGET"), "800x240 / 30 FPS TARGET", "800x240 / CIBLE 30 IPS", "800x240 / OBJETIVO 30 FPS"},
        {CaptionHash("400x240 / 60 FPS TARGET"), "400x240 / 60 FPS TARGET", "400x240 / CIBLE 60 IPS", "400x240 / OBJETIVO 60 FPS"},
        {CaptionHash("400x240 / 30 FPS TARGET"), "400x240 / 30 FPS TARGET", "400x240 / CIBLE 30 IPS", "400x240 / OBJETIVO 30 FPS"},
    };
    const unsigned key = CaptionHash(original);
    for(size_t i=0;i<sizeof(captions)/sizeof(captions[0]);++i)
        if(captions[i].key==key && !strcmp(captions[i].english,original))
            return language=='F' ? captions[i].french : captions[i].spanish;
    return NULL;
}
static inline bool Equals(const unsigned short* text, const char* ascii)
{
    while(*text && *ascii) { if(*text++ != Next(ascii)) return false; }
    return !*text && !*ascii;
}
static inline const char* Replacement(unsigned hash, unsigned modulo, const unsigned short* original, char language = 'E')
{
    const Entry* table = language == 'F' ? frenchEntries : language == 'S' ? spanishEntries : entries;
    const size_t count = language == 'F' ? sizeof(frenchEntries)/sizeof(Entry) :
        language == 'S' ? sizeof(spanishEntries)/sizeof(Entry) : sizeof(entries)/sizeof(Entry);
    for(size_t i = 0; i < count; ++i)
        if(Hash(table[i].key, modulo) == hash) return table[i].text;
    // Exact keycap captions only: narrative and mission text are untouched.
    if(Equals(original, "[ESC]")) return "B";
    if(Equals(original, "[ÉCHAP]") || Equals(original, "[ECHAP]")) return "B";
    if(Equals(original, "[ENTER]")) return "A";
    if(Equals(original, "[ENTRÉE]") || Equals(original, "[ENTREE]")) return "A";
    if(Equals(original, "[ESC] Quit to Main Menu")) return language == 'F' ? "B : Menu principal" :
        language == 'S' ? "B: Menú principal" : "B: Main Menu";
    return NULL;
}
}
#endif
