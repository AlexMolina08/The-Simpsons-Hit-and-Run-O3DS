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
    {"CHECKING_MEMORY_CARDS_(PC)", "Checking for saved games..."},
    {"PRESS_START", "PRESS START"},
    {"PRESS_START_GC", "PRESS START"},
    {"PRESS_START_PS2", "PRESS START"},
    {"PRESS_START_XBOX", "PRESS START"},
};
static const Entry frenchEntries[] = {
    {"TUTORIAL_000", "POUR DÉTRUIRE UNE GUÊPE CAMÉRA, SAUTE AVEC A, PUIS FRAPPE AVEC Y."},
    {"TUTORIAL_002", "DÉPLACE-TOI AVEC LE PAD CIRCULAIRE.\\A : SAUTER. B : COURIR.\\L + PAD CIRCULAIRE : CAMÉRA."},
    {"TUTORIAL_002_(PS2)", "PAD CIRCULAIRE : SE DÉPLACER.\\A : SAUTER. B : COURIR."},
    {"TUTORIAL_003", "A : ACCÉLÉRER.\\PAD CIRCULAIRE : TOURNER.\\B : FREINER OU RECULER.\\Y OU R : FREIN DE PARKING."},
    {"TUTORIAL_003_(GC)", "A : ACCÉLÉRER. PAD : TOURNER. B : FREINER OU RECULER. Y OU R : FREIN DE PARKING."},
    {"TUTORIAL_003_(PS2)", "A : ACCÉLÉRER. PAD : TOURNER. B : FREINER OU RECULER. Y OU R : FREIN DE PARKING."},
    {"TUTORIAL_004", "APPUIE SUR X POUR ENTRER DANS UN VÉHICULE OU EN SORTIR."},
    {"TUTORIAL_008", "Utilise les pièces pour acheter des voitures et des vêtements."},
    {"TUTORIAL_009", "APPUIE SUR X POUR UTILISER LA CABINE TÉLÉPHONIQUE."},
    {"TUTORIAL_010", "APPUIE SUR X POUR ENTRER DANS UN VÉHICULE OU EN SORTIR."},
    {"TUTORIAL_011", "APPUIE SUR Y POUR FRAPPER LA CAISSE."},
    {"TUTORIAL_012", "Casse certains objets pour obtenir des pièces."},
    {"TUTORIAL_013", "APPUIE SUR X POUR ENTRER."},
    {"TUTORIAL_014", "Pièce trouvée"},
    {"TUTORIAL_015", "Véhicule détruit !\\Trouve une clé ou va à une cabine téléphonique pour le réparer."},
    {"TUTORIAL_016", "Véhicule débloqué\\Va à la cabine téléphonique pour y accéder à tout moment."},
    {"TUTORIAL_017", "Utilise les clés pour réparer la voiture."},
    {"TUTORIAL_018", "APPUIE SUR X POUR ENTRER DANS UN VÉHICULE OU EN SORTIR."},
    {"TUTORIAL_019", "Gag à proximité"},
    {"TUTORIAL_020", "Remporte la course contre la montre pour gagner des pièces supplémentaires."},
    {"TUTORIAL_021", "Entrée d'apprentissage incorrecte"},
    {"CHECKING_MEMORY_CARDS_(PC)", "Recherche de sauvegardes..."},
    {"PRESS_START", "APPUIE SUR START"},
    {"PRESS_START_GC", "APPUIE SUR START"},
    {"PRESS_START_PS2", "APPUIE SUR START"},
    {"PRESS_START_XBOX", "APPUIE SUR START"},
};
static const Entry spanishEntries[] = {
    {"TUTORIAL_000", "PARA DESTRUIR UNA CÁMARA AVISPA, SALTA CON A Y GOLPEA CON Y."},
    {"TUTORIAL_002", "MUÉVETE CON EL BOTÓN DESLIZANTE.\\A: SALTAR. B: CORRER.\\L + BOTÓN DESLIZANTE: CÁMARA."},
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
    {"CHECKING_MEMORY_CARDS_(PC)", "Buscando partidas guardadas..."},
    {"PRESS_START", "PULSA START"},
    {"PRESS_START_GC", "PULSA START"},
    {"PRESS_START_PS2", "PULSA START"},
    {"PRESS_START_XBOX", "PULSA START"},
};
// Corrections for retail French strings exported without their accents.
static const Entry frenchCorrections[] = {
    {"PARLEZ A MARGE", "Parlez à Marge"},
    {"PARLEZ A NED", "Parlez à Ned"},
    {"PARLEZ A LENNY", "Parlez à Lenny"},
    {"PARLEZ A HANS MOLEMAN", "Parlez à Hans Moleman"},
    {"PARLEZ A SMITHERS", "Parlez à Smithers"},
    {"PARLEZ A JIMBO", "Parlez à Jimbo"},
    {"PARLEZ A LISA", "Parlez à Lisa"},
    {"PARLEZ A APU", "Parlez à Apu"},
    {"PARLEZ A CARL", "Parlez à Carl"},
    {"PARLEZ A OTTO", "Parlez à Otto"},
    {"PARLEZ A MILHOUSE", "Parlez à Milhouse"},
    {"PARLEZ A KEARNEY", "Parlez à Kearney"},
    {"PARLEZ A BURNS", "Parlez à Burns"},
    {"PARLEZ A KRUSTY", "Parlez à Krusty"},
    {"PARLEZ A HOMER", "Parlez à Homer"},
    {"PARLEZ A BART", "Parlez à Bart"},
    {"PARLEZ A NELSON", "Parlez à Nelson"},
    {"PARLEZ A CLETUS", "Parlez à Cletus"},
    {"PARLEZ A GRAND-PERE", "Parlez à Grand-père"},
    {"PARLEZ A BARNEY", "Parlez à Barney"},
    {"PARLEZ A RALPH", "Parlez à Ralph"},
    {"PARLEZ A JASPER", "Parlez à Jasper"},
    {"PARLEZ A MOE", "Parlez à Moe"},
    {"TERMINEZ LA COURSE AVANT LA FIN DU TEMPS IMPARTI. NE DETRUISEZ PAS VOTRE VEHICULE.", "Terminez la course avant la fin du temps imparti. Ne détruisez pas votre véhicule."},
    {"TERMINEZ LA COURSE EN PREMIERE POSITION. NE DETRUISEZ PAS VOTRE VEHICULE.", "Terminez la course en première position. Ne détruisez pas votre véhicule."},
    {"PASSEZ PAR TOUS LES POINTS DE PASSAGE ET TERMINEZ PREMIER. NE DETRUISEZ PAS VOTRE VEHICULE.", "Passez par tous les points de passage et terminez premier. Ne détruisez pas votre véhicule."},
    {"FAITES UN TOUR COMPLET DANS LE TEMPS IMPARTI POUR GAGNER DES PIECES.", "Faites un tour complet dans le temps imparti pour gagner des pièces."},
    {"IL VOUS FAUT %D PIECES POUR PARTICIPER A LA COURSE", "Il vous faut %D pièces pour participer à la course"},
    {"ENTREZ DANS LE VEHICULE.", "Entrez dans le véhicule."},
    {"DETRUISEZ LE VEHICULE DE SMITHERS AVANT QU'IL ARRIVE A LA CENTRALE.", "Détruisez le véhicule de Smithers avant qu'il arrive à la centrale."},
    {"ENTREZ DANS VOTRE VEHICULE", "Entrez dans votre véhicule"},
    {"PRENEZ UN VEHICULE A BART", "Prenez un véhicule à Bart"},
    {"VOUS DEVEZ ACHETER DES VETEMENTS COOLS DANS LE DONJON DE L'ANDROIDE OU A L'OBSERVATOIRE.", "Vous devez acheter des vêtements cools dans le Donjon de l'Androïde ou à l'Observatoire."},
    {"ACHETEZ UN VEHICULE PERSONNALISE A HOMER. IL EST SUR LE PARKING DE L'HOPITAL.", "Achetez un véhicule personnalisé à Homer. Il est sur le parking de l'hôpital."},
    {"VOUS DEVEZ ACHETER DES VETEMENTS AMERICAINS CHEZ MOE OU AU DMV.", "Vous devez acheter des vêtements américains chez Moe ou au DMV."},
    {"ACHETEZ DES VETEMENTS DE PRISONNIER AU KWIK-E-MART OU DANS LA CHAMBRE DE BART.", "Achetez des vêtements de prisonnier au Kwik-E-Mart ou dans la chambre de Bart."},
    {"PASSEZ L'ENTREE DE LA CENTRALE", "Passez l'entrée de la centrale"},
    {"ALLEZ A L'ENTREE DE L'HOPITAL", "Allez à l'entrée de l'hôpital"},
    {"DETRUISEZ LA VOITURE DU CONSERVATEUR POUR AVOIR LA CLE DU MUSEE", "Détruisez la voiture du conservateur pour avoir la clé du musée"},
    {"ENTREZ DANS UN VEHICULE.", "Entrez dans un véhicule."},
    {"REGARDEZ VOTRE NOUVEAU COSTUME DANS LA FENETRE.", "Regardez votre nouveau costume dans la fenêtre."},
    {"SORTEZ DU VEHICULE - VITE !", "Sortez du véhicule - vite !"},
    {"VEHICULE", "VÉHICULE"},
    {"VEHICULES", "VÉHICULES"},
    {"OPTIONS VEHICULE", "OPTIONS VÉHICULE"},
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
    if(!original || (language != 'F' && language != 'S')) return NULL;
    struct CaptionEntry {unsigned key; const char* english; const char* french; const char* spanish;};
    static const CaptionEntry captions[] = {
        {CaptionHash("GRAPHICS"), "GRAPHICS", "GRAPHISMES", "GRÁFICOS"},
        {CaptionHash("CHEATS"), "CHEATS", "CODES", "TRUCOS"},
        {CaptionHash("PHONE BOOTH"), "PHONE BOOTH", "CABINE TÉLÉPHONIQUE", "CABINA TELEFÓNICA"},
        {CaptionHash("SHOP"), "SHOP", "BOUTIQUE", "TIENDA"},
        {CaptionHash("LOADING"), "LOADING", "CHARGEMENT", "CARGANDO"},
        {CaptionHash("PREVIEW UNAVAILABLE"), "PREVIEW UNAVAILABLE", "Aperçu indisponible", "VISTA NO DISPONIBLE"},
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
        {CaptionHash("NINTENDO 3DS CONTROLS"), "NINTENDO 3DS CONTROLS", "COMMANDES NINTENDO 3DS", "CONTROLES DE NINTENDO 3DS"},
        {CaptionHash("A: Jump / gas\nB: Run / brake\nX: Interact / car\nY: Attack\nR: Handbrake\nPad: Move / steer"),
            "A: Jump / gas\nB: Run / brake\nX: Interact / car\nY: Attack\nR: Handbrake\nPad: Move / steer",
            "A : Saut / accélérer\nB : Courir / freiner\nX : Action / voiture\nY : Frapper\nR : Frein à main\nPad : Bouger / tourner",
            "A: Saltar / acelerar\nB: Correr / frenar\nX: Acción / coche\nY: Golpear\nR: Freno de mano\nPad: Mover / girar"},
        {CaptionHash("L + Pad: Camera\nD-pad: Move / steer\nSELECT: Reset car\nL + SELECT: View\nL + R: Horn\nSTART: Pause"),
            "L + Pad: Camera\nD-pad: Move / steer\nSELECT: Reset car\nL + SELECT: View\nL + R: Horn\nSTART: Pause",
            "L + Pad : Caméra\nCroix : Déplacer\nSELECT : Replacer\nL + SELECT : Vue\nL + R : Klaxon\nSTART : Pause",
            "L + Pad: Cámara\nCruceta: Mover\nSELECT: Recolocar\nL + SELECT: Vista\nL + R: Claxon\nSTART: Pausa"},
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
        {CaptionHash("+100 COINS"), "+100 COINS", "+100 pièces", "+100 MONEDAS"},
        {CaptionHash("INVINCIBLE CAR"), "INVINCIBLE CAR", "VÉHICULE INVINCIBLE", "COCHE INVENCIBLE"},
        {CaptionHash("NO SPEED LIMIT"), "NO SPEED LIMIT", "VITESSE ILLIMITÉE", "VELOCIDAD ILIMITADA"},
        {CaptionHash("FAST ACCELERATION"), "FAST ACCELERATION", "ACCÉLÉRATION RAPIDE", "ACELERACIÓN RÁPIDA"},
        {CaptionHash("HORN JUMP"), "HORN JUMP", "KLAXON POUR SAUTER", "SALTAR CON EL CLAXON"},
        {CaptionHash("ONE-HIT TRAFFIC"), "ONE-HIT TRAFFIC", "UN COUP SUFFIT", "TRÁFICO DE UN GOLPE"},
        {CaptionHash("WRECKED CAR LOOK"), "WRECKED CAR LOOK", "ASPECT ENDOMMAGÉ", "ASPECTO DESTROZADO"},
        {CaptionHash("RED BRICK CARS"), "RED BRICK CARS", "VÉHICULES EN BRIQUES", "COCHES DE LADRILLOS"},
        {CaptionHash("KICK SWAPS MODEL"), "KICK SWAPS MODEL", "Frapper change le modèle", "PATADA CAMBIA MODELO"},
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
        {CaptionHash("CONTINUE"), "CONTINUE", "CONTINUER", "CONTINUAR"},
        {CaptionHash("SELECT"), "SELECT", "CHOISIR", "ELEGIR"},
        {CaptionHash("CANCEL"), "CANCEL", "ANNULER", "CANCELAR"},
        {CaptionHash("START MISSION"), "START MISSION", "COMMENCER", "INICIAR MISIÓN"},
        {CaptionHash("CALL CAR"), "CALL CAR", "APPELER", "LLAMAR COCHE"},
        {CaptionHash("REPAIR & CALL"), "REPAIR & CALL", "RÉPARER ET APPELER", "REPARAR Y LLAMAR"},
        {CaptionHash("BUY"), "BUY", "ACHETER", "COMPRAR"},
        {CaptionHash("ROTATE LEFT"), "ROTATE LEFT", "ROTATION GAUCHE", "GIRAR IZQUIERDA"},
        {CaptionHash("ROTATE RIGHT"), "ROTATE RIGHT", "ROTATION DROITE", "GIRAR DERECHA"},
        {CaptionHash("X HIDE TUTORIALS"), "X HIDE TUTORIALS", "X MASQUER CONSEILS", "X OCULTAR AYUDAS"},
        {CaptionHash("Thank you so much for playing."), "Thank you so much for playing.", "Merci beaucoup d'avoir joué.", "Muchas gracias por jugar."},
        {CaptionHash("Bringing this game to Nintendo 3DS has"), "Bringing this game to Nintendo 3DS has", "Adapter ce jeu sur Nintendo 3DS a été", "Llevar este juego a Nintendo 3DS ha"},
        {CaptionHash("been a challenging and rewarding journey."), "been a challenging and rewarding journey.", "un défi exigeant et passionnant.", "sido un viaje difícil y gratificante."},
        {CaptionHash("I hope you have enjoyed playing this as"), "I hope you have enjoyed playing this as", "J'espère que vous avez pris autant", "Espero que hayas disfrutado jugando"},
        {CaptionHash("much as I have enjoyed bringing it to life."), "much as I have enjoyed bringing it to life.", "de plaisir à jouer que moi à créer.", "tanto como yo al darle vida."},
        {CaptionHash("23 October 2026"), "23 October 2026", "23 octobre 2026", "23 de octubre de 2026"},
        {CaptionHash("\"It always seems impossible until it's done.\""), "\"It always seems impossible until it's done.\"", "\"Tout semble impossible jusqu'à ce qu'on le fasse.\"", "\"Siempre parece imposible hasta que se hace.\""},
        {CaptionHash("3DS adaptation made by"), "3DS adaptation made by", "Adaptation 3DS réalisée par", "Adaptación para 3DS de"},
        {CaptionHash("INTERACT"), "INTERACT", "INTERAGIR", "INTERACTUAR"},
        {CaptionHash("TALK"), "TALK", "PARLER", "HABLAR"},
        {CaptionHash("ENTER VEHICLE"), "ENTER VEHICLE", "MONTER", "ENTRAR EN COCHE"},
        {CaptionHash("GO INSIDE"), "GO INSIDE", "ENTRER", "ENTRAR"},
        {CaptionHash("BUY VEHICLE"), "BUY VEHICLE", "ACHETER", "COMPRAR COCHE"},
        {CaptionHash("OUTFITS"), "OUTFITS", "TENUES", "TRAJES"},
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
    if(language == 'F')
        for(size_t i = 0; i < sizeof(frenchCorrections)/sizeof(Entry); ++i)
            if(Equals(original, frenchCorrections[i].key)) return frenchCorrections[i].text;
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
