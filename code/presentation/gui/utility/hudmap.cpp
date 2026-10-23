#ifdef RAD_3DS
#include <main/graphics3ds.h>
#include <presentation/gui/frontend/guiscreencheats.h>
#include <presentation/gui/ingame/guiscreenmissionload.h>
#endif
#include <presentation/gui/frontend/guiscreenviewcredits.h>
#include <presentation/gui/utility/handheldcredits.h>
#include <presentation/gui/utility/handheldiconbounds.h>
#include <main/old3dsbottommap.h>
//===========================================================================
// Copyright (C) 2000 Radical Entertainment Ltd.  All rights reserved.
//
// Component:   CHudMap
//
// Description: Implementation of the CHudMap class.
//
// Authors:     Tony Chu
//
// Revisions		Date			Author	    Revision
//                  2002/08/01      TChu        Created for SRR2
//
//===========================================================================

//===========================================================================
// Includes
//===========================================================================
#include <presentation/gui/utility/hudmap.h>
#include <presentation/gui/utility/hudmapcam.h>
#include <presentation/gui/utility/specialfx.h>
#include <presentation/gui/guiscreen.h>

#include <ai/vehicle/vehicleai.h>
#include <interiors/interiormanager.h>
#include <memory/srrmemory.h>
#include <meta/zoneeventlocator.h>
#include <mission/gameplaymanager.h>
#include <render/IntersectManager/IntersectManager.h>
#include <roads/geometry.h>
#include <roads/road.h>
#include <roads/intersection.h>
#include <roads/roadmanager.h>
#include <worldsim/avatarmanager.h>
#include <worldsim/avatar.h>
#include <worldsim/hitnrunmanager.h>
#include <worldsim/redbrick/vehicle.h>

#include <p3d/camera.hpp>
#include <p3d/pointcamera.hpp>
#include <p3d/utility.hpp>

#include <raddebug.hpp> // Foundation

#include <Page.h>
#include <Group.h>
#include <Pure3dObject.h>
#include <Sprite.h>
#include <Polygon.h>
#ifdef RAD_3DS
#include <presentation/gui/guisystem.h>
#include <presentation/gui/guimanager.h>
#include <presentation/presentation.h>
#include <presentation/fmvplayer/fmvplayer.h>
#include <p3d/font.hpp>
#include <p3d/sprite.hpp>
#include <p3d/imagefactory.hpp>
#include <p3d/loadmanager.hpp>
#include <pddi/pica/bottomframe.hpp>
#include <presentation/gui/utility/handheldtext.h>
#include <presentation/gui/guitextbible.h>
#include <mission/mission.h>
#include <mission/missionstage.h>
#include <mission/charactersheet/charactersheetmanager.h>
#include <mission/rewards/rewardsmanager.h>
#include <ai/actionbuttonhandler.h>
#include <events/eventmanager.h>
#include <worldsim/character/character.h>
#include <worldsim/coins/coinmanager.h>
#include <p3d/shader.hpp>
#include <3ds.h>
extern "C" void picaBottomScreenMapping(const float*,float,float,float,float);
#ifdef SRR2_OLD_3DS
extern "C" void old3dsTouchActionRequest();
extern "C" void old3dsTouchActionCancel();
#endif
#include <presentation/gui/ingame/guimanageringame.h>
#include <presentation/gui/ingame/guiscreenhud.h>
#include <presentation/gui/ingame/guiscreenrewards.h>
#include <presentation/gui/ingame/guiscreenphonebooth.h>
#include <presentation/gui/utility/vehiclepreview.h>
#include <p3d/drawable.hpp>
#include <gameflow/gameflow.h>
#include <App.h>
#include <Layer.h>
#include <Screen.h>
#include <p3d/matrixstack.hpp>
#include <p3d/view.hpp>
extern "C" bool picaBottomScreenBegin( float u0, float v0, float u1, float v1 );
extern "C" void picaBottomScreenEnd( void );
extern "C" void picaBottomScreenFrame( void );
extern "C" void picaBottomScreenStatic( void );
extern "C" void picaBottomScreenPanel( int selection, int count );
extern "C" bool picaBottomScreenInterfaceBegin( void );
#ifdef SRR2_OLD_3DS
extern "C" bool picaStereoObjectBegin(float screenDistance);
extern "C" void picaStereoObjectEnd(void);
#endif
#endif

//===========================================================================
// Global Data, Local Data, Local Classes
//===========================================================================

#define SHOW_HUD_MAP
#define CIRCULAR_HUD_MAP

#ifdef CIRCULAR_HUD_MAP
    #define SMARTER_ICON_LOCATIONS // only works for circular hud map
#endif

// This is for smooth camera height transitions, to prevent any sudden
// camera cuts.
//
#define SMOOTH_CAMERA_HEIGHT_TRANSITIONS

const int NUM_AMORTIZED_UPDATE_FRAMES = 5; // for DetermineOnRoadLocation()

#ifdef DEBUGWATCH
    #include <raddebugwatch.hpp>

    static const char* WATCHER_NAMESPACE = "GUI System - HUD Map";

    float MIN_CAMERA_HEIGHT = 50.0f;
    float MAX_CAMERA_HEIGHT = 150.0f;

    float MAX_CAMERA_HEIGHT_TRANSITION = 10.0f;

    float CHASE_FLASH_DISTANCE_THRESHOLD = 0.5f;
    float CHASE_FADE_DISTANCE_THRESHOLD = 0.75f;
    float RADAR_VISIBLE_RADIUS = 150.0f; // in metres
#else
    const float MIN_CAMERA_HEIGHT = 50.0f;
    const float MAX_CAMERA_HEIGHT = 150.0f;

    const float MAX_CAMERA_HEIGHT_TRANSITION = 10.0f;

    const float CHASE_FLASH_DISTANCE_THRESHOLD = 0.5f;
    const float CHASE_FADE_DISTANCE_THRESHOLD = 0.75f;
    const float RADAR_VISIBLE_RADIUS = 150.0f; // in metres
#endif

const float DEFAULT_CAMERA_HEIGHT = 150.0f; // in metres

const float MAX_RADAR_CONE_ALPHA = 0.35f;

enum eVisibilityMode
{
    VISIBLE_IN_SUNDAY_DRIVE = 1,
    VISIBLE_IN_MISSION = 2,

    NUM_VISIBILITY_MODES
};

const tColour HIT_N_RUN_COLOUR_RED( 255, 0, 0 );
const tColour HIT_N_RUN_COLOUR_BLUE( 0, 0, 255 );

HudMapIcon CHudMap::s_registeredIcons[ MAX_NUM_REGISTERED_ICONS ];
int CHudMap::s_numRegisteredIcons = 0;
int CHudMap::s_fpIconID = -1;

//===========================================================================
// Public Member Functions
//===========================================================================

void
HudMapIcon::ApplyAICarIconColour()
{
    rAssert( m_iconImage != NULL );

    switch( m_type )
    {
        case ICON_AI_HIT_N_RUN:
        {
            m_iconImage->SetColour( HIT_N_RUN_COLOUR_RED );

            break;
        }
        case ICON_AI_CHASE:
        {
            m_iconImage->SetColour( tColour( 255, 128, 0 ) );

            break;
        }
        case ICON_AI_RACE:
        {
            m_iconImage->SetColour( tColour( 255, 255, 0 ) );

            break;
        }
        case ICON_AI_EVADE:
        {
            m_iconImage->SetColour( tColour( 255, 255, 0 ) );

            break;
        }
        case ICON_AI_TARGET:
        {
            m_iconImage->SetColour( tColour( 255, 0, 0 ) );

            break;
        }
        default:
        {
            rAssertMsg( false, "Invalid AI car icon type!" );

            break;
        }
    }
}

//===========================================================================
// CHudMap::CHudMap
//===========================================================================
// Description: Constructor.
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
CHudMap::CHudMap( Scrooby::Page* pPage, int playerID, const char* p3dFile )
:   m_playerID( playerID ),
    m_p3dMap( NULL ),
    m_p3dHole( NULL ),
    m_originalPosX( 0 ),
    m_originalPosY( 0 ),
    m_originalWidth( 0 ),
    m_originalHeight( 0 ),
    m_isVisible( false ),
    m_radar( NULL ),
    m_radarCone( NULL ),
    m_iconsGroup( NULL ),
#ifdef RAD_3DS
    m_hudMapGroup( NULL ),
    m_hudLayer( NULL ),
#endif
    m_hudMapCam( NULL ),
    m_currentCameraHeight( DEFAULT_CAMERA_HEIGHT ),
    m_fixedCameraHeight( 0.0f ),
    m_currentAICarDistance( 0.0f ),
    m_maxAICarDistance( 0.0f ),
    m_elapsedTime( 0 ),
    m_lastRoadSeg( NULL ),
    m_lastOnRoadLocation( 0.0f, 0.0f, 0.0f ),
    m_frameCount( -1 )
{
MEMTRACK_PUSH_GROUP( "CHudMap" );
    memset( m_icons, 0, sizeof( m_icons ) );

    m_hudMapCam = new HudMapCam( m_playerID );
    rAssert( m_hudMapCam );
    m_hudMapCam->AddRef();

    m_currentCameraHeight = this->CalculateCameraHeight( RADAR_VISIBLE_RADIUS );

    char name[ 16 ];
    rAssert( pPage != NULL );

    // get 3D HUD map
    //
    sprintf( name, "Map%d", playerID );
    m_p3dMap = pPage->GetPure3dObject( name );
    rAssert( m_p3dMap != NULL );
    m_p3dMap->SetCamera( m_hudMapCam->GetCamera() );

    // store original hud map position and size
    //
    m_p3dMap->GetOriginPosition( m_originalPosX, m_originalPosY );
    m_p3dMap->GetBoundingBoxSize( m_originalWidth, m_originalHeight );

#ifdef SHOW_HUD_MAP
    if( p3dFile != NULL )
    {
        // set pure3d file for hud map
        //
        m_p3dMap->Add3dModel( p3dFile );
    }

    // get 3D HUD hole
    //
    sprintf( name, "Hole%d", playerID );
    m_p3dHole = pPage->GetPure3dObject( name );
    rAssert( m_p3dHole != NULL );
    m_p3dHole->SetCamera( m_hudMapCam->GetCamera() );
    m_p3dHole->SetColourWrite( false );

    if( CGuiScreen::IsWideScreenDisplay() )
    {
        m_p3dMap->SetWideScreenCorrectionEnabled( true );
        m_p3dHole->SetWideScreenCorrectionEnabled( true );
    }
#else
    m_p3dMap->SetVisible( false );
#endif

/*
    sprintf( name, "RadarLight%d", playerID );
    m_radarLight = pPage->GetSprite( name );
    rAssert( m_radarLight != NULL );

    // hide radar light temporarily
    //
    m_radarLight->SetVisible( false );
*/

    // get radar & radar cone
    //
    sprintf( name, "Radar%d", playerID );
    m_radar = pPage->GetGroup( name );
    rAssert( m_radar != NULL );

    sprintf( name, "RadarCone%d", playerID );
    m_radarCone = pPage->GetGroup( name );
    rAssert( m_radarCone != NULL );

    Scrooby::Polygon* radarConePolygon = m_radarCone->GetPolygon( name );
    rAssert( radarConePolygon != NULL );
    radarConePolygon->SetAlpha( MAX_RADAR_CONE_ALPHA );

    // get icons group
    //
    sprintf( name, "Icons%d", playerID );
    m_iconsGroup = pPage->GetGroup( name );
    rAssert( m_iconsGroup != NULL );

#ifdef RAD_3DS
    sprintf( name, "HudMap%d", playerID );
    m_hudMapGroup = pPage->GetGroup( name );
    m_hudLayer = pPage->GetLayer( "Foreground" );
#endif

#ifdef DEBUGWATCH
    ::radDbgWatchAddFloat( &MIN_CAMERA_HEIGHT,
                           "Mininum Camera Height",
                           WATCHER_NAMESPACE,
                           NULL, NULL, 30.0f, 90.0f );

    ::radDbgWatchAddFloat( &MAX_CAMERA_HEIGHT,
                           "Maximum Camera Height",
                           WATCHER_NAMESPACE,
                           NULL, NULL, 100.0f, 1000.0f );

    ::radDbgWatchAddFloat( &MAX_CAMERA_HEIGHT_TRANSITION,
                           "Maximum Camera Height Change",
                           WATCHER_NAMESPACE,
                           NULL, NULL, 1.0f, 50.0f );

    ::radDbgWatchAddFloat( &RADAR_VISIBLE_RADIUS,
                           "Radar Visible Radius",
                           WATCHER_NAMESPACE,
                           NULL, NULL, 0.0f, 1000.0f );

    ::radDbgWatchAddFloat( &CHASE_FLASH_DISTANCE_THRESHOLD,
                           "Chase Flash Distance (%)",
                           WATCHER_NAMESPACE );

    ::radDbgWatchAddFloat( &CHASE_FADE_DISTANCE_THRESHOLD,
                           "Chase Fade Distance (%)",
                           WATCHER_NAMESPACE );
#endif
MEMTRACK_POP_GROUP("CHudMap");
}

//===========================================================================
// CHudMap::~CHudMap
//===========================================================================
// Description: Destructor.
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
CHudMap::~CHudMap()
{
#ifdef DEBUGWATCH
    ::radDbgWatchDelete( &MIN_CAMERA_HEIGHT );
    ::radDbgWatchDelete( &MAX_CAMERA_HEIGHT );
    ::radDbgWatchDelete( &MAX_CAMERA_HEIGHT_TRANSITION );
    ::radDbgWatchDelete( &RADAR_VISIBLE_RADIUS );
    ::radDbgWatchDelete( &CHASE_FLASH_DISTANCE_THRESHOLD );
    ::radDbgWatchDelete( &CHASE_FADE_DISTANCE_THRESHOLD );
#endif

    if( m_hudMapCam != NULL )
    {
        m_hudMapCam->Release();
        m_hudMapCam = NULL;
    }
}

//===========================================================================
// CHudMap::SetCameraTarget
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
void
CHudMap::SetCameraTarget( ISuperCamTarget* target )
{
    rAssert( m_hudMapCam );
    m_hudMapCam->SetTarget( target );
}

void
CHudMap::EnableFixedCameraHeight( bool enable, float visibleRadius )
{
/*
    if( !enable )
    {
        rWarningMsg( false, "Can't disable fixed camera height in new radar implementation!" );

        // ignore request to disable fixed camera height
        //
        return;
    }
*/
    m_fixedCameraHeight = enable ?
                          visibleRadius / rmt::Tan( m_hudMapCam->GetFOV() / 2.0f ) :
                          0.0f;
}

//===========================================================================
// CHudMap::Update
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
void
CHudMap::Update( unsigned int elapsedTime )
{
    if( GetInteriorManager()->IsInside() )
    {
        this->SetVisible( false );
    }

    if( !m_isVisible )
    {
        // no need to update if not visible
        //
        return;
    }

/*
    // update radar light (just rotate about radar center point)
    //
    static float RADAR_ROTATION_PERIOD = 2000.0f; // in msec
    float rotation = (elapsedTime / RADAR_ROTATION_PERIOD) * 360.0f;
    rAssert( m_radarLight != NULL );
    m_radarLight->RotateAboutCenter( rotation );
*/

    // get current hud map position and size
    //
    int mapX = 0;
    int mapY = 0;
    int mapWidth = 0;
    int mapHeight = 0;

    rAssert( m_p3dMap != NULL );
    m_p3dMap->GetOriginPosition( mapX, mapY );
    m_p3dMap->GetBoundingBoxSize( mapWidth, mapHeight );

    float newCameraHeight = DEFAULT_CAMERA_HEIGHT;
    if( m_fixedCameraHeight != 0.0f ) // fixed camera height enabled
    {
        newCameraHeight = m_fixedCameraHeight;
    }
    else
    {
        if( s_fpIconID != -1 )
        {
            HudMapIcon* focalPointIcon = &(s_registeredIcons[ s_fpIconID ]);
            rAssert( focalPointIcon->m_iconImage );

            if( focalPointIcon->m_dynamicLocator != NULL )
            {
                focalPointIcon->m_dynamicLocator->GetPosition( &(focalPointIcon->m_location) );
            }

            newCameraHeight = this->CalculateCameraHeight( s_registeredIcons[ 0 ].m_location,
                                                           focalPointIcon->m_location );
        }
    }

#ifdef SMOOTH_CAMERA_HEIGHT_TRANSITIONS
    // for smoother camera height transitions
    //
    float cameraHeightChange = newCameraHeight - m_currentCameraHeight;
    if( rmt::Abs( cameraHeightChange ) > MAX_CAMERA_HEIGHT_TRANSITION )
    {
        m_currentCameraHeight += rmt::Sign( cameraHeightChange ) * MAX_CAMERA_HEIGHT_TRANSITION;
//        rTunePrintf( "Current camera height = %f m\n", m_currentCameraHeight );
    }
    else
#endif
    {
        m_currentCameraHeight = newCameraHeight;
    }

    // update hud map camera
    //
    rAssert( m_hudMapCam != NULL );
    m_hudMapCam->SetHeight( m_currentCameraHeight );
    m_hudMapCam->SetAspect( (float)mapWidth / (float)mapHeight );
    m_hudMapCam->Update( elapsedTime );

    rmt::Vector hudMapCenter;
    m_hudMapCam->GetPosition( &hudMapCenter );

#ifdef CIRCULAR_HUD_MAP
    // update hud hole
    //
    if( m_p3dHole != NULL )
    {
        static float awayFromCam = 4.5f;
        m_p3dHole->SetDrawableTranslation( hudMapCenter.x,
                                           hudMapCenter.y - awayFromCam,
                                           hudMapCenter.z );
    }
#endif

    // update registered icons
    //
    int iconPosX = 0;
    int iconPosY = 0;
    int iconWidth = 0;
    int iconHeight = 0;

    const unsigned int PULSE_PERIOD = 500;
//    bool iconBlinked = false;

//    rWarningMsg( s_fpIconID != -1, "No active focal point icon on hud map!" );

    for( unsigned int i = 0; i < MAX_NUM_REGISTERED_ICONS; i++ )
    {
        if( s_registeredIcons[ i ].m_iconImage != NULL )
        {
            // check if visible in current gameplay mode
            //
            if( GetGameplayManager()->IsSundayDrive() )
            {
                if( (s_registeredIcons[ i ].m_visibilityMask & VISIBLE_IN_SUNDAY_DRIVE) == 0 )
                {
                    s_registeredIcons[ i ].m_iconImage->SetVisible( false );
                    continue;
                }
            }
            else
            {
                if( (s_registeredIcons[ i ].m_visibilityMask & VISIBLE_IN_MISSION) == 0 )
                {
                    s_registeredIcons[ i ].m_iconImage->SetVisible( false );
                    continue;
                }
            }

            // hide icons that are not visible when player is in car
            //
            if( s_registeredIcons[ i ].m_type == HudMapIcon::ICON_PHONE_BOOTH ||
                s_registeredIcons[ i ].m_type == HudMapIcon::ICON_PLAYER_CAR )
            {
                Avatar* player = GetAvatarManager()->GetAvatarForPlayer( m_playerID );
                rAssert( player != NULL );
                if( player->IsInCar() )
                {
                    s_registeredIcons[ i ].m_iconImage->SetVisible( false );
                    continue;
                }
            }

//            s_registeredIcons[ i ].m_iconImage->SetAlpha( 1.0f );

            // center icon in hud map first
            //
            s_registeredIcons[ i ].m_iconImage->GetBoundingBoxSize( iconWidth, iconHeight );
            iconPosX = (mapX + mapWidth / 2) - (iconWidth / 2);
            iconPosY = (mapY + mapHeight / 2) - (iconHeight / 2);
            s_registeredIcons[ i ].m_iconImage->SetPosition( iconPosX, iconPosY );

            s_registeredIcons[ i ].m_iconImage->ResetTransformation();

            // update current location for dynamic icons
            if( s_registeredIcons[ i ].m_dynamicLocator != NULL )
            {
                s_registeredIcons[ i ].m_dynamicLocator->GetPosition( &s_registeredIcons[ i ].m_location );

                // place icon object at sea level
                s_registeredIcons[ i ].m_location.y = 0;

                if( s_registeredIcons[ i ].IsAICarIcon() ||
                    s_registeredIcons[ i ].m_type == HudMapIcon::ICON_PLAYER ||
                    s_registeredIcons[ i ].m_type == HudMapIcon::ICON_PLAYER_CAR )
                {
                    rmt::Vector heading( 0, 0, 0 );
                    s_registeredIcons[ i ].m_dynamicLocator->GetHeading( &heading );

                    this->UpdateIconHeading( s_registeredIcons[ i ].m_iconImage, &heading );
                }
            }

            rmt::Vector iconLocInWorld = s_registeredIcons[ i ].m_location;

#ifdef SMARTER_ICON_LOCATIONS
            if( static_cast< int >( i ) == s_fpIconID )
            {
                // TC: [TODO] need to determine this from the maximum camera height and the
                //            clipping boundary value
                //
                static float visibleRadius = 120.0f;

                this->DetermineOnRoadLocation( hudMapCenter,
                                               iconLocInWorld,
                                               visibleRadius );
            }
#endif

            rmt::Vector iconLoc( 0, 0, 0 );
            rAssert( m_p3dMap->GetCamera() );
            m_p3dMap->GetCamera()->WorldToView( iconLocInWorld, &iconLoc );
            iconLoc.z = 0.0f;

            static float CLIPPING_BOUNDARY = 0.40f;
            float iconScale = 1.0f;

#ifdef CIRCULAR_HUD_MAP
            // apply view clipping (for circular view)
            //
            float iconLength = iconLoc.Magnitude();
            if( iconLength > CLIPPING_BOUNDARY )
            {
                iconScale = CLIPPING_BOUNDARY / iconLength;
                iconLoc.Scale( iconScale );
            }
#else
            // apply view clipping (for rectangular view)
            //

            // x-clipping
            float xScale = 1.0f;
            if( iconLoc.x > CLIPPING_BOUNDARY )
            {
                xScale = CLIPPING_BOUNDARY / iconLoc.x;
                iconLoc.Scale( xScale, xScale, 1 );
            }
            else if( iconLoc.x < -CLIPPING_BOUNDARY )
            {
                xScale = -CLIPPING_BOUNDARY / iconLoc.x;
                iconLoc.Scale( xScale, xScale, 1 );
            }

            // y-clipping
            float yScale = 1.0f;
            if( iconLoc.y > CLIPPING_BOUNDARY )
            {
                yScale = CLIPPING_BOUNDARY / iconLoc.y;
                iconLoc.Scale( yScale, yScale, 1 );
            }
            else if( iconLoc.y < -CLIPPING_BOUNDARY )
            {
                yScale = -CLIPPING_BOUNDARY / iconLoc.y;
                iconLoc.Scale( yScale, yScale, 1 );
            }

            iconScale = xScale * yScale;
#endif

            bool isIconClipped = (iconScale < 1.0f);

            rAssert( m_radarCone != NULL );
            m_radarCone->SetVisible( s_fpIconID != -1 );

            if( static_cast< int >( i ) == s_fpIconID ) // for focal point icon ...
            {
                // update radar cone direction
                //
                m_radarCone->ResetTransformation();
                m_radarCone->RotateAboutCenter( rmt::RadianToDeg( this->CalculatRadarConeAngle( iconLoc ) ) );

                if( !isIconClipped || s_registeredIcons[ i ].IsAICarIcon() )
                {
                    m_radarCone->SetAlpha( iconScale );
                }
                else
                {
                    const float MINIMUM_ICON_SCALE = 0.75f;
                    float alpha = (iconScale - MINIMUM_ICON_SCALE) / (1.0f - MINIMUM_ICON_SCALE);
                    m_radarCone->SetAlpha( alpha > 0.0f ? alpha : 0.0f );
                }
/*
                // apply alpha if focal point icon is clipped on boundary
                //
                if( iconScale < 1.0f )
                {
                    if( iconScale < 0.3f )
                    {
                        s_registeredIcons[ i ].m_iconImage->SetAlpha( 0.3f );
                    }
                    else
                    {
                        s_registeredIcons[ i ].m_iconImage->SetAlpha( iconScale );
                    }
                }
                else
                {
                    s_registeredIcons[ i ].m_iconImage->SetAlpha( 1.0f );
                }
*/

#ifdef SMARTER_ICON_LOCATIONS
                s_registeredIcons[ i ].m_iconImage->SetVisible( true );
#else
                if( !s_registeredIcons[ i ].IsAICarIcon() )
                {
                    // hide non-AI car icons if clipped on boundary
                    //
                    s_registeredIcons[ i ].m_iconImage->SetVisible( !isIconClipped );
                }
#endif

                // special cases
                //
                if( s_registeredIcons[ i ].m_type == HudMapIcon::ICON_MISSION )
                {
                    static float SCALE_AMPLITUDE = 0.25f;

                    float scale = GuiSFX::Pulse( (float)m_elapsedTime,
                                                 (float)PULSE_PERIOD,
                                                 1.0f + SCALE_AMPLITUDE,
                                                 SCALE_AMPLITUDE );

/*
                    float scale = 1.0f + (float)m_elapsedTime / PULSE_PERIOD * SCALE_AMPLITUDE;
                    float alpha = 1.0f - (float)m_elapsedTime / PULSE_PERIOD * ALPHA_AMPLITUDE;

                    s_registeredIcons[ i ].m_iconImage->SetAlpha( alpha );
*/
                    s_registeredIcons[ i ].m_iconImage->ScaleAboutCenter( scale );
                }
/*
                else if( s_registeredIcons[ i ].m_type == HudMapIcon::ICON_AI_CAR )
                {
                    // if focal point is AI car icon, fade out when near
                    // distance limit
                    //
                    if( m_maxAICarDistance > 0.0f )
                    {
                        float distanceRatio = m_currentAICarDistance / m_maxAICarDistance;
                        rAssert( distanceRatio <= 1.0f );

                        if( distanceRatio > CHASE_FLASH_DISTANCE_THRESHOLD )
                        {
                            // blink icon
                            //
                            iconBlinked = GuiSFX::Blink( s_registeredIcons[ i ].m_iconImage,
                                                         (float)m_elapsedTime,
                                                         (float)BLINKING_PERIOD );

                            if( distanceRatio > CHASE_FADE_DISTANCE_THRESHOLD )
                            {
                                float iconAlpha = (1.0f - distanceRatio) /
                                                  (1.0f - CHASE_FADE_DISTANCE_THRESHOLD);

                                s_registeredIcons[ i ].m_iconImage->SetAlpha( iconAlpha );
                            }
                            else
                            {
                                s_registeredIcons[ i ].m_iconImage->SetAlpha( 1.0f );
                            }
                        }
                        else
                        {
                            s_registeredIcons[ i ].m_iconImage->SetVisible( true );
                        }
                    }
                }
*/
            }
            else // for non-focal point icons ...
            {
                if( !s_registeredIcons[ i ].IsAICarIcon() )
                {
                    // hide non-AI car icons if clipped on boundary
                    //
                    s_registeredIcons[ i ].m_iconImage->SetVisible( !isIconClipped );
                }
/*
                // for phone booth icons
                //
                if( s_registeredIcons[ i ].m_type == HudMapIcon::ICON_PHONE_BOOTH )
                {
                    // only show if within visible radius
                    //
                    rmt::Vector playerToPhone;
                    playerToPhone.Sub( s_registeredIcons[ i ].m_location,
                                       s_registeredIcons[ 0 ].m_location );

                    float distanceRatio = playerToPhone.Magnitude() / PHONE_BOOTH_VISIBLE_RADIUS;
                    bool isWithinRadius = (distanceRatio < 1.0f);

                    // fade out nicely to prevent popping
                    //
                    const float FADE_OUT_THRESHOLD = 0.9f;
                    if( !isIconClipped && isWithinRadius )
                    {
                        if( distanceRatio > FADE_OUT_THRESHOLD )
                        {
                            float iconAlpha = (1.0f - distanceRatio) / (1.0f - FADE_OUT_THRESHOLD);
                            s_registeredIcons[ i ].m_iconImage->SetAlpha( iconAlpha );
                        }
                        else
                        {
                            s_registeredIcons[ i ].m_iconImage->SetAlpha( 1.0f );
                        }
                    }
                    else
                    {
                        s_registeredIcons[ i ].m_iconImage->SetAlpha( 0.0f );
                    }
                }
*/
            }

            if( s_registeredIcons[ i ].m_type == HudMapIcon::ICON_AI_HIT_N_RUN )
            {
                if( GetHitnRunManager()->IsHitnRunActive() )
                {
                    s_registeredIcons[ i ].m_iconImage->SetVisible( true );

                    // modulate icon colour
                    //
                    tColour iconColour;
                    GuiSFX::ModulateColour( &iconColour,
                                            (float)m_elapsedTime,
                                            (float)PULSE_PERIOD,
                                            HIT_N_RUN_COLOUR_RED,
                                            HIT_N_RUN_COLOUR_BLUE );

                    s_registeredIcons[ i ].m_iconImage->SetColour( iconColour );
                }
                else
                {
                    // hide icon if H&R is not active
                    //
                    s_registeredIcons[ i ].m_iconImage->SetVisible( false );
                }
            }

            // translate the icon (in screen space wrt. hud map center)
            //
            int iconX = static_cast<int>( iconLoc.x * mapWidth );
            int iconY = static_cast<int>( iconLoc.y * mapHeight );

            s_registeredIcons[ i ].m_iconImage->Translate( iconX, iconY );
        }
    }
/*
    if( iconBlinked )
    {
        m_elapsedTime %= PULSE_PERIOD;
    }
*/
    m_elapsedTime = (m_elapsedTime + elapsedTime) % PULSE_PERIOD;
}

//===========================================================================
// CHudMap::AddIconToInventory
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
void
CHudMap::AddIconToInventory( HudMapIcon::eIconType type, Scrooby::Sprite* image )
{
    if( image == NULL )
    {
        return;
    }

    unsigned int i = 0;

    for( i = 0; i < MAX_NUM_ICONS_PER_TYPE; i++ )
    {
        // find first empty slot in inventory to add icon
        //
        if( m_icons[ type ][ i ] == NULL )
        {
            m_icons[ type ][ i ] = image;

            // hide icon image
            //
            image->SetVisible( false );

            break;
        }
    }

    rAssertMsg( i < MAX_NUM_ICONS_PER_TYPE,
                "ERROR: *** Exceeded maximum number of icons per type!\n" );
}

//===========================================================================
// CHudMap::RemoveIconFromInventory
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
Scrooby::Sprite*
CHudMap::RemoveIconFromInventory( HudMapIcon::eIconType type )
{
    Scrooby::Sprite* iconImage = NULL;

    // find an available icon of specified type in inventory
    //
    for( unsigned int i = 0; i < MAX_NUM_ICONS_PER_TYPE; i++ )
    {
        if( m_icons[ type ][ i ] != NULL )
        {
            iconImage = m_icons[ type ][ i ];
            iconImage->SetVisible( true );

            // free up slot in inventory
            //
            m_icons[ type ][ i ] = NULL;

            break;
        }
    }

    return iconImage;
}

//===========================================================================
// CHudMap::RegisterIcon
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
int
CHudMap::RegisterIcon( HudMapIcon::eIconType type,
                       rmt::Vector location,
                       IHudMapIconLocator* hudMapIconLocator,
                       bool newFocalPoint )
{
    rAssert( s_numRegisteredIcons < static_cast<int>( MAX_NUM_REGISTERED_ICONS ) );

    int iconID = -1;

    Scrooby::Sprite* iconImage = this->RemoveIconFromInventory( type );
    if( iconImage == NULL )
    {
        rAssertMsg( false, "WARNING: *** No more available icons of this type! Please tell Tony." );

        return -1;
    }

    // convert locations in interiors to world-space location of the interior
    //
    tName interiorName = GetInteriorManager()->ClassifyPoint( location );
    if( interiorName.GetUID() != static_cast< tUID >( 0 ) )
    {
        ZoneEventLocator* locator = p3d::find<ZoneEventLocator>( interiorName.GetUID() );
        locator->GetPosition( &location );

        if( hudMapIconLocator != NULL )
        {
            hudMapIconLocator = locator;
        }
    }

#ifdef RAD_DEBUG
    // check if there's already a static icon of the same type
    // registered at exactly the same location
    //
    for( unsigned int j = 0; j < MAX_NUM_REGISTERED_ICONS; j++ )
    {
        if( s_registeredIcons[ j ].m_iconImage != NULL &&
            s_registeredIcons[ j ].m_type == type )
        {
            rAssertMsg( s_registeredIcons[ j ].m_dynamicLocator != NULL ||
                        s_registeredIcons[ j ].m_location != location,
                        "WARNING: *** Another icon of this type has already been registered at this location. Is this by design??" );
        }
    }
#endif // RAD_DEBUG

    for( unsigned int i = 0; i < MAX_NUM_REGISTERED_ICONS; i++ )
    {
        if( i == 0 && type != HudMapIcon::ICON_PLAYER )
        {
            // icon ID = 0 is reserved for player icon
            continue;
        }

        // find first empty slot to register icon
        if( s_registeredIcons[ i ].m_iconImage == NULL )
        {
            iconID = i;
            s_numRegisteredIcons++;

            // place icon object at sea level
            location.y = 0;

            s_registeredIcons[ iconID ].m_iconImage = iconImage;
            s_registeredIcons[ iconID ].m_type = type;
            s_registeredIcons[ iconID ].m_location = location;
            s_registeredIcons[ iconID ].m_dynamicLocator = hudMapIconLocator;

            // set visible in sunday drive mode for these icon types
            //
            if( type == HudMapIcon::ICON_PLAYER ||
                type == HudMapIcon::ICON_PLAYER_CAR ||
                type == HudMapIcon::ICON_MISSION ||
                type == HudMapIcon::ICON_BONUS_MISSION ||
                type == HudMapIcon::ICON_STREET_RACE ||
                type == HudMapIcon::ICON_WAGER_RACE ||
                type == HudMapIcon::ICON_PURCHASE_CENTRE ||
                type == HudMapIcon::ICON_PHONE_BOOTH ||
                type == HudMapIcon::ICON_AI_HIT_N_RUN )
            {
                s_registeredIcons[ i ].m_visibilityMask |= VISIBLE_IN_SUNDAY_DRIVE;
            }

            // set visible in mission mode for these icon types
            //
            if( type == HudMapIcon::ICON_PLAYER ||
                type == HudMapIcon::ICON_PLAYER_CAR ||
                type == HudMapIcon::ICON_FLAG_CHECKERED ||
                type == HudMapIcon::ICON_FLAG_WAYPOINT ||
                type == HudMapIcon::ICON_COLLECTIBLE ||
                type == HudMapIcon::ICON_MISSION ||
                s_registeredIcons[ i ].IsAICarIcon() )
            {
                s_registeredIcons[ i ].m_visibilityMask |= VISIBLE_IN_MISSION;
            }

            // colour code AI car icons
            //
            if( s_registeredIcons[ i ].IsAICarIcon() )
            {
                s_registeredIcons[ i ].ApplyAICarIconColour();
            }

            break;
        }
    }

    rAssertMsg( iconID != -1, "ERROR: *** There should have been an empty slot!\n" );

    if( newFocalPoint )
    {
        // set as new focal point icon
        //
        s_fpIconID = iconID;

        // whenever the target changes, we want to recompute its closest road segment
        m_lastRoadSeg = NULL;

        m_frameCount = -1; // reset frame count
    }


    return iconID;
}

//===========================================================================
// CHudMap::UnregisterIcon
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
void
CHudMap::UnregisterIcon( int iconID )
{
    if( iconID != -1 )
    {
        rAssert( iconID < static_cast<int>( MAX_NUM_REGISTERED_ICONS ) );

        if( s_registeredIcons[ iconID ].m_iconImage != NULL )
        {
            // return icon to inventory
            //
            this->AddIconToInventory( s_registeredIcons[ iconID ].m_type,
                                      s_registeredIcons[ iconID ].m_iconImage );

            s_registeredIcons[ iconID ].m_iconImage = NULL;
            s_registeredIcons[ iconID ].m_visibilityMask = 0;
            s_numRegisteredIcons--;

            if( s_fpIconID == iconID )
            {
                s_fpIconID = -1;
            }
        }
    }
    else
    {
        rAssertMsg( false, "This should only happen if you didn't get a valid icon ID upon registration!" );
    }
}

//=============================================================================
// CHudMap::ChangeIconType
//=============================================================================
// Description: Comment
//
// Parameters:  ( HudMapIcon::eIconType type, int iconID )
//
// Return:      void 
//
//=============================================================================
int
CHudMap::ChangeIconType( int iconID, HudMapIcon::eIconType type )
{
    int newIconID = -1;

    if( iconID != -1 )
    {
        bool isFocalPointIcon = (iconID == s_fpIconID);

        // save a copy of old icon
        //
        HudMapIcon* oldIcon = &s_registeredIcons[ iconID ];

        // unregister it first
        //
        this->UnregisterIcon( iconID );

        // re-registered icon of new type
        //
        newIconID = this->RegisterIcon( type, oldIcon->m_location, oldIcon->m_dynamicLocator, isFocalPointIcon );
    }

    return newIconID;
}

//===========================================================================
// CHudMap::SetFocalPointIcon
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
void
CHudMap::SetFocalPointIcon( int iconID )
{
    if( iconID != -1 )
    {
        rAssert( iconID < static_cast< int >( MAX_NUM_REGISTERED_ICONS ) );
        s_fpIconID = iconID;

        // whenever the target changes, we want to recompute its closest road segment
        m_lastRoadSeg = NULL;

        m_frameCount = -1; // reset frame count
    }
}

//===========================================================================
// CHudMap::FindIcon
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
HudMapIcon*
CHudMap::FindIcon( HudMapIcon::eIconType type ) const
{
    const HudMapIcon* icon = NULL;

    for( unsigned int i = 0; i < MAX_NUM_REGISTERED_ICONS; i++ )
    {
        if( s_registeredIcons[ i ].m_iconImage != NULL &&
            s_registeredIcons[ i ].m_type == type )
        {
            // found it!
            //
            icon = &(s_registeredIcons[ i ]);

            break;
        }
    }

    return const_cast<HudMapIcon*>( icon );
}

//===========================================================================
// CHudMap::Translate
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
void CHudMap::Translate( int x, int y )
{
    rAssert( m_p3dMap );

    m_originalPosX += x;
    m_originalPosY += y;
    m_p3dMap->SetPosition( m_originalPosX, m_originalPosY );
    if( m_p3dHole != NULL )
    {
        m_p3dHole->SetPosition( m_originalPosX, m_originalPosY );
    }

    this->Update( 0 );
}

//===========================================================================
// CHudMap::Reset
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
void CHudMap::Reset()
{
    rAssert( m_p3dMap != NULL );

    // restore hud map position and size
    //
    m_p3dMap->SetPosition( m_originalPosX, m_originalPosY );
    m_p3dMap->SetBoundingBoxSize( m_originalWidth, m_originalHeight );

    if( m_p3dHole != NULL )
    {
        m_p3dHole->SetPosition( m_originalPosX, m_originalPosY );
        m_p3dHole->SetBoundingBoxSize( m_originalWidth, m_originalHeight );
    }

    // restore hud map camera
    //
    m_p3dMap->SetCamera( m_hudMapCam->GetCamera() );

    m_elapsedTime = 0;

    this->Update( 0 );
}

//===========================================================================
// CHudMap::SetVisible
//===========================================================================
// Description: Toggle visibility of 3D map.
//
// Constraints:	None.
//
// Parameters:	visibility
//
// Return:      
//
//===========================================================================
void CHudMap::SetVisible( bool isVisible )
{
    m_isVisible = isVisible;

    rAssert( m_p3dMap != NULL );
    m_p3dMap->SetVisible( isVisible );

    rAssert( m_radar != NULL );
    m_radar->SetVisible( isVisible );

    rAssert( m_iconsGroup != NULL );
    m_iconsGroup->SetVisible( isVisible );
}

#ifdef RAD_3DS
namespace
{
    CHudMap*        s_bottomMap        = NULL;
    Scrooby::Group* s_bottomBackground = NULL;
    bool            s_mapWasVisible        = false;
    bool            s_backgroundWasVisible = false;
    Scrooby::Page* s_bottomDialogPages[8];
    bool s_bottomDialogVisible[8];
    unsigned s_bottomDialogCount=0;
    picaBottomFrame::TouchOwner s_touchOwner;
    bool s_hudActionPressed=false;
    picaBottomFrame::MenuScroll s_mainScroll;
    bool s_mainDragging=false;
    unsigned s_mainEpoch=~0u;
    int s_mainCount=0;
    float MainScrollPosition(CGuiMenu* menu)
    {
        const int count=menu->GetMenuItem(0)->m_itemValueCount;
        const int row=picaBottomFrame::MainRow(menu->GetSelectionValue(0),count);
        const unsigned now=static_cast<unsigned>(osGetTime());
        if(s_mainEpoch!=s_touchOwner.epoch || s_mainCount!=count){
            s_mainEpoch=s_touchOwner.epoch;s_mainCount=count;s_mainDragging=false;
            s_mainScroll=picaBottomFrame::MenuScroll();
            s_mainScroll.Drag(float(row>4?row-4:0),count,now);
        }
        if(!s_mainDragging)s_mainScroll.Follow(row,count,now);
        return s_mainScroll.Value(now);
    }

    bool s_phonePreviewVisible=false;

    CGuiScreen* CurrentBottomScreen()
    {
        CGuiManager* manager=GetGuiSystem()->GetCurrentManager();
        return manager ? dynamic_cast<CGuiScreen*>(manager->GetCurrentWindow()) : NULL;
    }


    Scrooby::Text* MenuLabel(GuiMenuItem* item)
    {
        return dynamic_cast<Scrooby::Text*>(item->GetItem());
    }

    int MenuRows(CGuiMenu* menu,int (&rows)[64])
    {
        int count=0;
        if( menu && menu->GetNumItems()<=64 ) for( int i=0;i<menu->GetNumItems();++i )
        {
            Scrooby::Text* label=MenuLabel(menu->GetMenuItem(i));
            if( label && label->GetFont() && label->IsVisible() && menu->GetMenuItem(i)->m_attributes!=ALL_ATTRIBUTES_OFF ) rows[count++]=i;
        }
        return count;
    }

    int MenuSprites(CGuiMenu* menu,int (&rows)[64])
    {
        int count=0;
        if(menu && menu->GetNumItems()<=64)for(int i=0;i<menu->GetNumItems();++i){
            Scrooby::Sprite* sprite=dynamic_cast<Scrooby::Sprite*>(menu->GetMenuItem(i)->GetItem());
            if(sprite && sprite->IsVisible())rows[count++]=i;
        }
        return count;
    }

    Scrooby::Drawable* s_bottomActions[128];
    bool s_bottomActionVisible[128];
    float s_bottomActionAlpha[128];
    unsigned s_bottomActionCount=0;
    void HideBottomAction(Scrooby::Drawable* item)
    {
        if( !item || s_bottomActionCount>=128 ) return;
        for( unsigned i=0;i<s_bottomActionCount;++i ) if(s_bottomActions[i]==item)return;
        s_bottomActions[s_bottomActionCount]=item;
        s_bottomActionVisible[s_bottomActionCount]=item->IsVisible();
        s_bottomActionAlpha[s_bottomActionCount++]=item->GetAlpha();
        item->SetVisible(false);
    }

    CHudMap* CurrentMap()
    {
        const ContextEnum ctx = GetGameFlow()->GetCurrentContext();
        if( ( ctx != CONTEXT_GAMEPLAY && ctx != CONTEXT_PAUSE ) || GetCurrentHud() == NULL )
        {
            return NULL;
        }
        return GetCurrentHud()->GetHudMap( 0 );
    }

}

void CHudMap::BeforeRenderLayers3DS()
{
    s_bottomMap = NULL;
    s_bottomBackground = NULL;
    s_mapWasVisible = false;
    s_backgroundWasVisible = false;
    s_bottomDialogCount=0;
    s_phonePreviewVisible=false;
    CGuiScreen* screen=CurrentBottomScreen();
    if(s_touchOwner.Update(screen,screen?screen->HasMenu():NULL))s_hudActionPressed=false;
#ifdef SRR2_OLD_3DS
    if(!screen || screen->GetWindowID()!=CGuiWindow::GUI_SCREEN_ID_HUD)old3dsTouchActionCancel();
#endif
    s_bottomActionCount=0;
    CGuiMenu* menu=screen ? screen->HasMenu() : NULL;
    int rows[64];const int rowCount=MenuRows(menu,rows);
    for( int row=0;row<rowCount;++row )
    {
        if( screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_MAIN_MENU && rows[row]!=0 ) continue;
        GuiMenuItem* item=menu->GetMenuItem(rows[row]);
        HideBottomAction(item->GetItem());HideBottomAction(item->GetItemValue());
        HideBottomAction(item->m_itemValueArrowL);HideBottomAction(item->m_itemValueArrowR);
        HideBottomAction(item->m_slider.m_pPolygon);HideBottomAction(item->m_slider.m_pImage);
    }
    // Menu-owned decoration moves with its controls: slider rails, speaker
    // icons and toggle ornaments must not remain floating over the upper view.
    if(screen && rowCount>0 && screen->GetWindowID()!=CGuiWindow::GUI_SCREEN_ID_MAIN_MENU){
        Scrooby::Screen* resource=screen->GetScroobyScreen();
        for(int i=0;i<resource->GetNumberOfPages();++i)
            HideBottomAction(resource->GetPageByIndex(i)->GetGroup("Menu"));
    }
    int images[64];const int imageCount=MenuSprites(menu,images);
    for(int i=0;i<imageCount;++i)HideBottomAction(menu->GetMenuItem(images[i])->GetItem());
    if( screen && screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_LOADING )
    {
        Scrooby::Page* page=screen->GetScroobyScreen()->GetPage("Loading");
        if( page )
        {
            HideBottomAction(page->GetGroup("LoadingBar"));
            HideBottomAction(page->GetText("Loading"));
        }
    }
    if( screen && (screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PAUSE_SUNDAY ||
                   screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PAUSE_MISSION) )
    {
        // Only the root pause menu has moved all its board content downstairs.
        // Detail screens keep their own boards, artwork and mission information.
        HideBottomAction(screen->GetScroobyScreen()->GetPage("XSmallBoard"));
    }
    if( screen && screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_LEVEL_STATS )
    {
        Scrooby::Screen* resource=screen->GetScroobyScreen();
        HideBottomAction(resource->GetPage("LevelStats"));
        HideBottomAction(resource->GetPage("XSmallBoard"));
        HideBottomAction(resource->GetPage("Continue"));
        HideBottomAction(resource->GetPage("Back"));
    }
    if(screen && screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_SCRAP_BOOK_STATS){
        Scrooby::Page* page=screen->GetScroobyScreen()->GetPage("ScrapBookStats");
        if(page){HideBottomAction(page->GetGroup("Stats"));HideBottomAction(page->GetText("GameStats"));}
        HideBottomAction(screen->GetScroobyScreen()->GetPage("Back"));
    }
    if( screen && screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_TUTORIAL )
    {
        // Tutorials and the newspaper use the lower display without duplication.
        Scrooby::Screen* resource=screen->GetScroobyScreen();
        for( int i=0; resource && i<resource->GetNumberOfPages() && i<8; ++i )
        {
            Scrooby::Page* page=resource->GetPageByIndex(i);
            s_bottomDialogPages[i]=page;s_bottomDialogVisible[i]=page->IsVisible();
            page->SetVisible(false);++s_bottomDialogCount;
        }
    }

    if(screen && screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_HUD) {
        Scrooby::Screen* resource=screen->GetScroobyScreen();
        Scrooby::Page* page=resource->GetPage("Hud");
        if(page){HideBottomAction(page->GetGroup("Message"));HideBottomAction(page->GetGroup("ActionButton"));
            HideBottomAction(page->GetGroup("MissionObjective"));
            // The shared bitmap counter also belongs to the original card
            // reward sequence. Preserve that sequence; only coins move below.
            Scrooby::Group* collected=page->GetGroup("ItemCollected");
            Scrooby::Group* card=collected?collected->GetGroup("CardText"):NULL;
            if(!card || !card->IsVisible())HideBottomAction(page->GetGroup("ItemsCount"));}
        HideBottomAction(resource->GetPage("Coins"));
    }
    if(screen){
        const auto id=screen->GetWindowID();
        Scrooby::Screen* resource=screen->GetScroobyScreen();
        if(id==CGuiWindow::GUI_SCREEN_ID_MISSION_LOAD || id==CGuiWindow::GUI_SCREEN_ID_MISSION_SUCCESS){
            const char* names[]={"MissionInfo","MissionTitle","Loading"};
            for(const char* name:names)for(int i=0;i<resource->GetNumberOfPages();++i)HideBottomAction(resource->GetPageByIndex(i)->GetText(name));
            Scrooby::Page* page=resource->GetPage("MissionLoad");
            if(page)HideBottomAction(page->GetGroup("LoadCompleted"));
        }
        if(id==CGuiWindow::GUI_SCREEN_ID_PHONE_BOOTH || id==CGuiWindow::GUI_SCREEN_ID_PURCHASE_REWARDS){
            Scrooby::Page* page=resource->GetPage(id==CGuiWindow::GUI_SCREEN_ID_PHONE_BOOTH?"PhoneBooth":"Rewards");
            if(page){HideBottomAction(page->GetGroup("VehicleStats"));HideBottomAction(page->GetGroup("ToggleView"));
                HideBottomAction(page->GetGroup("DamagedInfo"));HideBottomAction(page->GetGroup("Locked"));}
            HideBottomAction(resource->GetPage("Coins"));
        }
        if(screen->HasMenu() || id==CGuiWindow::GUI_SCREEN_ID_MISSION_LOAD || id==CGuiWindow::GUI_SCREEN_ID_MISSION_SUCCESS){
            HideBottomAction(resource->GetPage("Continue"));HideBottomAction(resource->GetPage("Back"));HideBottomAction(resource->GetPage("Accept"));
            if(screen->HasMenu())HideBottomAction(screen->HasMenu()->GetCursor());
        }
    }
    if(screen){
        const auto id=screen->GetWindowID();
        Scrooby::Screen* resource=screen->GetScroobyScreen();
        if(id==CGuiWindow::GUI_SCREEN_ID_GENERIC_PROMPT || id==CGuiWindow::GUI_SCREEN_ID_ERROR_PROMPT || id==CGuiWindow::GUI_SCREEN_ID_MISSION_OVER){
            const char* names[]={"Message","MissionFailureReason","MissionFailureHint","RetryMission"};
            for(const char* name:names)for(int i=0;i<resource->GetNumberOfPages();++i)HideBottomAction(resource->GetPageByIndex(i)->GetText(name));
            HideBottomAction(resource->GetPage("MessageBox"));
        }
        if(id==CGuiWindow::GUI_SCREEN_ID_PHONE_BOOTH){
            Scrooby::Page* page=resource->GetPage("PhoneBooth");
            Scrooby::Sprite* image=page?page->GetSprite("PreviewImage"):NULL;
            s_phonePreviewVisible=image && image->IsVisible();
            HideBottomAction(image);
        }
        if(id==CGuiWindow::GUI_SCREEN_ID_PAUSE_SUNDAY || id==CGuiWindow::GUI_SCREEN_ID_PAUSE_MISSION){
            HideBottomAction(resource->GetPage("Pause"));HideBottomAction(resource->GetPage("Coins"));
            HideBottomAction(resource->GetPage("PauseMissionObjective"));
        }
        if(id==CGuiWindow::GUI_SCREEN_ID_VIEW_CARDS || id==CGuiWindow::GUI_SCREEN_ID_CARD_GALLERY){
            const char* names[]={"CardTitle","CardEpisode","CardDescription"};
            if(!menu)for(const char* name:names)for(int i=0;i<resource->GetNumberOfPages();++i)HideBottomAction(resource->GetPageByIndex(i)->GetText(name));
        }
    }
    if(screen && screen->HasMenu()){
        Scrooby::Screen* resource=screen->GetScroobyScreen();
        const char* boards[]={"XSmallBoard","SmallBoard","BigBoard"};
        for(const char* name:boards)HideBottomAction(resource->GetPage(name));
    }
    if(screen && GetGameFlow()->GetCurrentContext()==CONTEXT_PAUSE){
        Scrooby::Page* page=screen->GetScroobyScreen()->GetPage("PauseBgd");
        Scrooby::Layer* dim=page?page->GetLayer("Background"):NULL;
        if(dim){HideBottomAction(dim);dim->SetVisible(true);dim->SetAlpha(0.55f);}
    }
    if(screen && screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_LETTER_BOX)
        HideBottomAction(screen->GetScroobyScreen()->GetPage("LetterBoxButtons"));
    if(screen && screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_MISSION_SELECT)
        HideBottomAction(screen->GetScroobyScreen()->GetPage("MissionSelect"));
    CHudMap* map = CurrentMap();
    if( map == NULL || map->m_hudMapGroup == NULL )
    {
        return;
    }
    s_bottomMap = map;

    Scrooby::Page* bgdPage = GetCurrentHud()->GetScroobyScreen()->GetPage( "HudBgd" );
    s_bottomBackground = ( bgdPage != NULL ) ? bgdPage->GetGroup( "HudMap0" ) : NULL;

    s_mapWasVisible = map->m_hudMapGroup->IsVisible();
    map->m_hudMapGroup->SetVisible( false );
    if( s_bottomBackground != NULL )
    {
        s_backgroundWasVisible = s_bottomBackground->IsVisible();
        s_bottomBackground->SetVisible( false );
    }
}

namespace
{
    // Draw existing pages without Screen::Display(), which also advances animation.
    void DisplayBottomDrawable( Scrooby::Drawable* drawable )
    {
        if( drawable == NULL || !drawable->IsVisible() ) return;
        float x=0, y=0;
        p3d::stack->Push();
        p3d::stack->Multiply( *drawable->GetTransform() );
        drawable->GetNormalizedPosition( x, y );
        p3d::stack->Translate( x, y, 0 );
        drawable->Display();
        p3d::stack->Pop();
    }

    void FlushUI();

    void BottomLabel( tFont* font, const P3D_UNICODE* text, float x, float y,
                      float height, tColour colour, float maxWidth=230 )
    {
        FlushUI();
        if( font == NULL || text == NULL || font->GetFontHeight() <= 0 ) return;
        float scale=height/(320.0f*font->GetFontHeight());
        const float width=font->GetTextWidth(text);
        if( width*scale > maxWidth/320 ) scale=maxWidth/(320*width);
        const tColour original=font->GetColour();
        p3d::stack->Push();
        // Native pixel anchors, single coverage pass: tiny drop shadows blur strokes.
        p3d::stack->Translate( floorf(x+0.5f)/320, (240-floorf(y+0.5f))/320, 0 );
        p3d::stack->Scale( scale, scale, 1 );
        font->SetColour( colour );font->DisplayText( text );
        p3d::stack->Pop();
        font->SetColour( original );
    }

    const tColour Ink(26,48,78),Paper(247,237,195),Gold(255,216,48),White(255,253,235);

    CGuiMenu* s_pressedMenu=NULL;
    int s_pressedItem=-1;
    u64 s_focusTick=0;
    int s_focusSelection=-1;
    CGuiScreen* s_focusScreen=NULL;
    int s_textScrollDelta=0;
    bool Pressed(CGuiMenu* menu,int index){return menu->HasSelectionBeenMade() || (s_pressedMenu==menu && s_pressedItem==index);}

    bool ChangesValue(CGuiMenu* menu)
    {
        GuiMenuItem* item=menu && menu->GetSelection()>=0?menu->GetMenuItem(menu->GetSelection()):NULL;
        return item && (item->m_slider.m_pImage || item->m_slider.m_pPolygon ||
            (item->m_itemValueCount>1 && !(item->m_attributes&SELECTABLE)));
    }

    bool PauseGrid(CGuiScreen* screen)
    {
        return screen && (screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PAUSE_SUNDAY ||
            screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PAUSE_MISSION);
    }

    struct UIQuad {float l,t,r,b;tColour colour;};
    UIQuad s_quads[64];unsigned s_quadCount=0;
    void FlushUI()
    {
        if(!s_quadCount)return;
        static pddiShader* shader=NULL;
        if(!shader){shader=p3d::device->NewShader("simple");shader->AddRef();
            shader->SetInt(PDDI_SP_ISLIT,0);shader->SetInt(PDDI_SP_BLENDMODE,PDDI_BLEND_ALPHA);}
        pddiPrimStream* stream=p3d::pddi->BeginPrims(shader,PDDI_PRIM_TRIANGLES,PDDI_V_C,6*s_quadCount);
        if(stream){
            const unsigned corners[6]={0,1,2,2,1,3};
            for(unsigned i=0;i<s_quadCount;++i){const auto& q=s_quads[i];
                for(unsigned c:corners){stream->Colour(q.colour);stream->Coord((c&1?q.r:q.l)/320,(240-(c&2?q.b:q.t))/320,0);}}
            p3d::pddi->EndPrims(stream);
        }
        s_quadCount=0;
    }
    void Fill(float l,float t,float r,float b,tColour colour)
    {
        if(s_quadCount==64)FlushUI();
        s_quads[s_quadCount++]={l,t,r,b,colour};
    }

    void Disc(float x,float y,float radius,tColour colour)
    {
        FlushUI();
        static pddiShader* shader=NULL;
        if(!shader){shader=p3d::device->NewShader("simple");shader->AddRef();
            shader->SetInt(PDDI_SP_ISLIT,0);shader->SetInt(PDDI_SP_BLENDMODE,PDDI_BLEND_ALPHA);}
        pddiPrimStream* stream=p3d::pddi->BeginPrims(shader,PDDI_PRIM_TRIANGLES,PDDI_V_C,48);
        if(!stream)return;
        for(unsigned i=0;i<16;++i){
            const float a=i*0.392699082f,b=(i+1)*0.392699082f;
            stream->Colour(colour);stream->Coord(x/320,(240-y)/320,0);
            stream->Colour(colour);stream->Coord((x+cosf(a)*radius)/320,(240-y-sinf(a)*radius)/320,0);
            stream->Colour(colour);stream->Coord((x+cosf(b)*radius)/320,(240-y-sinf(b)*radius)/320,0);
        }
        p3d::pddi->EndPrims(stream);
    }

    tSprite* UIAsset(const char* name)
    {
        // Reuse loaded retail sprites. Release project references on manager
        // changes, before the next level competes for Old 3DS texture memory.
        struct Slot {tUID id;tSprite* sprite;};
        static Slot slots[40]={};static unsigned count=0;static CGuiManager* owner=NULL;
        CGuiManager* manager=GetGuiSystem()->GetCurrentManager();
        if(owner!=manager){for(unsigned i=0;i<count;++i)slots[i].sprite->Release();count=0;owner=manager;}
        const tUID id=tEntity::MakeUID(name);
        for(unsigned i=0;i<count;++i)if(slots[i].id==id)return slots[i].sprite;
        tSprite* sprite=p3d::find<tSprite>(name);
        if(sprite && count<40){sprite->AddRef();slots[count++]={id,sprite};}
        return sprite;
    }

    void RetailArt(tSprite* sprite,float x,float y,float w,float h,tColour tint=tColour(255,255,255))
    {
        FlushUI();
        if(!sprite || sprite->GetWidth()<=0 || sprite->GetHeight()<=0)return;
        const tColour old=sprite->GetColour();sprite->SetColour(tint);
        p3d::stack->Push();p3d::stack->Translate((x-w*0.5f)/320,(240-y-h*0.5f)/320,0);
        // Retail tSprite geometry starts at the lower left and uses nativeX on both axes.
        p3d::stack->Scale(w*sprite->GetNativeX()/(320*sprite->GetWidth()),
            h*sprite->GetNativeX()/(320*sprite->GetHeight()),1);
        sprite->Display();p3d::stack->Pop();sprite->SetColour(old);
    }

    // Keep small retail icons at their authored size, with pixel-aligned bounds.
    void PixelArt(tSprite* sprite,float x,float y,float w,float h)
    {
        const float left=floorf(x-w*0.5f+0.5f),top=floorf(y-h*0.5f+0.5f);
        RetailArt(sprite,left+w*0.5f,top+h*0.5f,w,h);
    }

    bool Television(CGuiScreen* screen)
    {
        const ContextEnum context=GetGameFlow()->GetCurrentContext();
        return screen && (screen->HasMenu() || screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_VIEW_CREDITS) && context!=CONTEXT_GAMEPLAY && context!=CONTEXT_PAUSE && UIAsset("tvframe.png");
    }

    void Gradient(float l,float t,float r,float b,tColour top,tColour bottom)
    {
        FlushUI();
        static pddiShader* shader=NULL;
        if(!shader){shader=p3d::device->NewShader("simple");shader->AddRef();
            shader->SetInt(PDDI_SP_ISLIT,0);shader->SetInt(PDDI_SP_BLENDMODE,PDDI_BLEND_ALPHA);}
        pddiPrimStream* stream=p3d::pddi->BeginPrims(shader,PDDI_PRIM_TRIANGLES,PDDI_V_C,6);
        if(!stream)return;
        const unsigned corners[6]={0,1,2,2,1,3};
        for(unsigned c:corners){stream->Colour(c&2?bottom:top);stream->Coord((c&1?r:l)/320,(240-(c&2?b:t))/320,0);}
        p3d::pddi->EndPrims(stream);
    }

    void Rounded(float l,float t,float r,float b,tColour top,tColour bottom)
    {
        FlushUI();
        static pddiShader* shader=NULL;
        if(!shader){shader=p3d::device->NewShader("simple");shader->AddRef();
            shader->SetInt(PDDI_SP_ISLIT,0);shader->SetInt(PDDI_SP_BLENDMODE,PDDI_BLEND_ALPHA);}
        pddiPrimStream* stream=p3d::pddi->BeginPrims(shader,PDDI_PRIM_TRIANGLES,PDDI_V_C,48);
        if(!stream)return;
        const float radius=6,cx=(l+r)*0.5f,cy=(t+b)*0.5f;
        float x[16],y[16];
        const float offsets[4][2]={{1,0},{0.8660254f,0.5f},{0.5f,0.8660254f},{0,1}};
        for(unsigned corner=0;corner<4;++corner)for(unsigned i=0;i<4;++i){
            const float a=offsets[i][0]*radius,d=offsets[i][1]*radius;const unsigned n=corner*4+i;
            switch(corner){
                case 0:x[n]=r-radius+a;y[n]=b-radius+d;break;
                case 1:x[n]=l+radius-d;y[n]=b-radius+a;break;
                case 2:x[n]=l+radius-a;y[n]=t+radius-d;break;
                default:x[n]=r-radius+d;y[n]=t+radius-a;break;
            }
        }
        const auto vertex=[&](float vx,float vy){
            const float f=(vy-t)/(b-t);
            stream->Colour(tColour(int(top.Red()+(bottom.Red()-top.Red())*f),int(top.Green()+(bottom.Green()-top.Green())*f),
                int(top.Blue()+(bottom.Blue()-top.Blue())*f),int(top.Alpha()+(bottom.Alpha()-top.Alpha())*f)));
            stream->Coord(vx/320,(240-vy)/320,0);
        };
        for(unsigned i=0;i<16;++i){vertex(cx,cy);vertex(x[i],y[i]);vertex(x[(i+1)%16],y[(i+1)%16]);}
        p3d::pddi->EndPrims(stream);
    }

    bool RetailFrame(float l,float t,float r,float b,bool selected)
    {
        const char* names[]={"frame_tl.png","frame_tr.png","frame_bl.png","frame_br.png",
            "frame_t.png","frame_b.png","frame_l.png","frame_r.png"};
        tSprite* pieces[8];for(unsigned i=0;i<8;++i){pieces[i]=UIAsset(names[i]);if(!pieces[i])return false;}
        const tColour tint=selected?tColour(255,255,255):tColour(151,148,183);
        const float corner=9,edge=3;
        RetailArt(pieces[0],l+corner/2,t+corner/2,corner,corner,tint);
        RetailArt(pieces[1],r-corner/2,t+corner/2,corner,corner,tint);
        RetailArt(pieces[2],l+corner/2,b-corner/2,corner,corner,tint);
        RetailArt(pieces[3],r-corner/2,b-corner/2,corner,corner,tint);
        RetailArt(pieces[4],(l+r)/2,t+edge/2,r-l-2*corner,edge,tint);
        RetailArt(pieces[5],(l+r)/2,b-edge/2,r-l-2*corner,edge,tint);
        RetailArt(pieces[6],l+edge/2,(t+b)/2,edge,b-t-2*corner,tint);
        RetailArt(pieces[7],r-edge/2,(t+b)/2,edge,b-t-2*corner,tint);
        return true;
    }

    void Board(bool television=false,bool commands=true)
    {
        Gradient(0,0,320,240,tColour(66,53,101),tColour(22,21,46));
        tSprite* glass=UIAsset("mapbgd.png");
        const ContextEnum context=GetGameFlow()->GetCurrentContext();
        if(!glass && (context==CONTEXT_GAMEPLAY || context==CONTEXT_PAUSE) && GetCurrentHud()){
            Scrooby::Page* page=GetCurrentHud()->GetScroobyScreen()->GetPage("HudBgd");
            Scrooby::Sprite* background=page?page->GetSprite("MapBgd0"):NULL;
            if(background)glass=const_cast<tSprite*>(background->GetRawSprite());
        }
        RetailArt(glass,160,126,318,236,tColour(125,108,166));
        if(television){
            // Light comes from the picture tube, rather than separate buttons.
            Gradient(39,28,281,214,tColour(50,49,99),tColour(25,26,61));
            Gradient(43,60,277,129,tColour(73,66,121,65),tColour(84,79,145,115));
            Gradient(43,129,277,208,tColour(84,79,145,115),tColour(20,22,57,30));
            RetailArt(UIAsset("tvframe.png"),160,120,320,240);
        }
        // A shallow glass shelf separates commands from the content above.
        else if(commands)Rounded(9,215,311,237,tColour(51,48,78),tColour(24,24,46));
    }

    void ClipRegion(const picaBottomFrame::Rect& region)
    {
        FlushUI();
        const int w=p3d::display->GetWidth(),h=p3d::display->GetHeight();
        pddiRect clip;clip.left=region.left*w/320;clip.top=region.top*h/240;
        clip.right=(region.right*w+319)/320;clip.bottom=(region.bottom*h+239)/240;
        p3d::pddi->SetScissor(&clip);
    }

    void TVClip(bool enabled)
    {
        ClipRegion(enabled?picaBottomFrame::TVGlass:picaBottomFrame::Rect{0,0,320,240});
    }

    void TVFinish()
    {
        // Stable scan lines, edge falloff and the original glass reflection make
        // the controls read as a picture behind the original television bezel.
        for(int y=36;y<208;y+=2)Fill(43,y,277,y+1,tColour(0,0,0,14));
        for(int x=43;x<53;++x){const int alpha=(53-x)*4;Fill(x,35,x+1,208,tColour(4,2,13,alpha));Fill(319-x,35,320-x,208,tColour(4,2,13,alpha));}
        Gradient(43,35,277,45,tColour(4,2,13,60),tColour(4,2,13,0));
        Gradient(43,197,277,208,tColour(4,2,13,0),tColour(4,2,13,55));
        RetailArt(UIAsset("tvglass.png"),160,121.5f,234,173,tColour(255,255,255,24));
        TVClip(false);
        RetailArt(UIAsset("tvframe.png"),160,120,320,240);
    }

    float TileLift(bool selected,bool pressed)
    {
        if(!selected)return 0;
        if(pressed)return 1;
        const float t=s_focusTick?float(svcGetSystemTick()-s_focusTick)/SYSCLOCK_ARM11/0.14f:1;
        return -1-(t<1?sinf(t*3.14159265f)*(1-t)*2:0);

    }

    void Tile(const picaBottomFrame::Rect& bounds,bool selected,bool enabled,bool pressed=false)
    {
        const float lift=TileLift(selected,pressed);
        const float l=bounds.left,t=bounds.top+lift,r=bounds.right,b=bounds.bottom+lift;
        Rounded(l,t+3,r,b+3,tColour(7,6,17,110),tColour(7,6,17,80));
        const tColour high=!enabled?tColour(62,57,77):selected?tColour(255,242,128):tColour(88,75,125);
        const tColour low=!enabled?tColour(39,37,51):selected?tColour(232,172,25):tColour(43,34,72);
        Rounded(l,t,r,b,high,low);
        // Specular ridge and a separate lower edge give a readable press depth.
        Gradient(l+7,t+3,r-7,t+(b-t)*0.45f,selected?tColour(255,255,218,105):tColour(190,177,218,45),tColour(255,255,255,0));
        if(!RetailFrame(l,t,r,b,selected && enabled)){
            Fill(l+6,t,r-6,t+1,selected?Gold:tColour(130,115,164));
            Fill(l+6,b-1,r-6,b,tColour(12,9,26));
        }
    }

    void TVRow(const picaBottomFrame::Rect& bounds,bool selected,bool pressed)
    {
        if(!selected)return;
        const float inset=pressed?1:0;
        Gradient(bounds.left,bounds.top+inset,bounds.right,bounds.bottom,tColour(204,170,58,62),tColour(139,101,41,26));
        Fill(bounds.left+10,bounds.bottom-1,bounds.right-10,bounds.bottom,tColour(255,223,102,95));
    }

    void Ascii(tFont* font,const char* text,float x,float baseline,float height,tColour colour,float width=290)
    {
        P3D_UNICODE unicode[128];p3d::AsciiToUnicode(text,unicode,128);
        BottomLabel(font,unicode,x,baseline,height,colour,width);
    }

    void LoadingPanel(Scrooby::Text* label)
    {
        if(!label || !label->GetFont())return;
        tFont* font=label->GetFont();
        const P3D_UNICODE* text=label->GetStringBuffer();
        if(!text || font->GetFontHeight()<=0)return;
        Rounded(24,89,296,153,tColour(207,164,46),tColour(119,84,32));
        Rounded(26,91,294,150,Paper,tColour(229,211,157));
        Fill(44,100,276,101,tColour(255,253,235,180));
        // Reuse the localized retail label and its dot animation at native pixels.
        const float width=fminf(240,font->GetTextWidth(text)*22/font->GetFontHeight());
        const float baseline=120-11+22*font->GetFontBaseLine()/font->GetFontHeight();
        BottomLabel(font,text,(320-width)*0.5f,baseline,22,Ink,240);
    }

    void DirectionIcon(bool right,float x,float y)
    {
        tSprite* sprite=UIAsset(right?"Rarrow.png":"Larrow.png");
        if(!sprite)sprite=UIAsset(right?"Rarrow0.png":"Larrow0.png");
        if(sprite){RetailArt(sprite,x,y,16,16);return;}
        FlushUI();
        static pddiShader* shader=NULL;
        if(!shader){shader=p3d::device->NewShader("simple");shader->AddRef();
            shader->SetInt(PDDI_SP_ISLIT,0);shader->SetInt(PDDI_SP_BLENDMODE,PDDI_BLEND_ALPHA);}
        pddiPrimStream* stream=p3d::pddi->BeginPrims(shader,PDDI_PRIM_TRIANGLES,PDDI_V_C,3);
        if(stream){const float sign=right?1:-1;
            stream->Colour(White);stream->Coord((x-sign*5)/320,(240-y+6)/320,0);
            stream->Colour(Gold);stream->Coord((x+sign*5)/320,(240-y)/320,0);
            stream->Colour(tColour(189,145,37));stream->Coord((x-sign*5)/320,(240-y-6)/320,0);
            p3d::pddi->EndPrims(stream);}
    }

    void Wrapped(tFont* font,const P3D_UNICODE* text,float x,float top,float width,float height,
                 unsigned maxLines,tColour colour,float boxHeight=0,bool centered=false)
    {
        if(!font || !text || font->GetFontHeight()<=0)return;
        // Reflow only when content or allotted area changes. HUD is not a text layout engine per frame.
        struct Cache {tFont* font;float width,height,renderHeight;P3D_UNICODE text[512],lines[16][256];unsigned count,maxLines;int offset;};
        static Cache cache[4]={};static unsigned next=0;
        Cache* found=NULL;
        for(auto& c:cache)if(c.font==font && c.width==width && c.height==height && c.maxLines==maxLines){
            unsigned i=0;while(i<511 && text[i] && text[i]==c.text[i])++i;
            if(i<511 && text[i]==c.text[i]){found=&c;break;}
        }
        if(!found){
            found=&cache[next++%4];found->font=font;found->width=width;found->height=height;found->maxLines=maxLines;found->offset=0;
            unsigned i=0;while(text[i] && i<511){found->text[i]=text[i];++i;}found->text[i]=0;
            for(float candidate=height;candidate>=12;candidate-=1){
                found->renderHeight=candidate;
                found->count=HandheldText::Wrap(found->text,found->lines,[&](const P3D_UNICODE* line){
                    return font->GetTextWidth(line)*candidate/font->GetFontHeight()<=width;
                });
                if(found->count<=maxLines)break;
            }
        }
        const unsigned shown=found->count<maxLines?found->count:maxLines;
        if(boxHeight>0 && shown)top+=fmaxf(0,(boxHeight-shown*found->renderHeight-(shown-1)*3)*0.5f);
        if(found->count>maxLines && found->count<=16){
            found->offset+=s_textScrollDelta;s_textScrollDelta=0;
            if(found->offset<0)found->offset=0;
            if(found->offset>int(found->count-maxLines))found->offset=found->count-maxLines;
            const float track=maxLines*(found->renderHeight+3)-3;
            const float thumb=track*maxLines/found->count;
            const float position=(track-thumb)*found->offset/(found->count-maxLines);
            Fill(x+width+3,top,x+width+5,top+track,tColour(100,91,133));
            Fill(x+width+3,top+position,x+width+5,top+position+thumb,Gold);
        }
        for(unsigned row=0;row+found->offset<found->count && row<maxLines && row+found->offset<16;++row){
            const P3D_UNICODE* line=found->lines[row+found->offset];
            const float inset=centered?fmaxf(0,(width-font->GetTextWidth(line)*found->renderHeight/font->GetFontHeight())*0.5f):0;
            BottomLabel(font,line,x+inset,top+found->renderHeight*font->GetFontBaseLine()/font->GetFontHeight()+row*(found->renderHeight+3),found->renderHeight,colour,width);
        }
    }

    Scrooby::Text* FindText(Scrooby::Screen* resource,const char* name)
    {
        for(int i=0;resource && i<resource->GetNumberOfPages();++i){
            Scrooby::Text* text=resource->GetPageByIndex(i)->GetText(name);if(text)return text;
        }
        return NULL;
    }

    // Traverse visible ownership, never inventing content for a screen.
    Scrooby::Text* FirstText(Scrooby::Owner* owner)
    {
        if(!owner)return NULL;
        for(unsigned i=0;i<owner->Size();++i){
            Scrooby::Drawable* child=owner->GetChildDrawable(i);if(!child || !child->IsVisible())continue;
            Scrooby::Text* text=dynamic_cast<Scrooby::Text*>(child);
            if(text && text->GetFont())return text;
            Scrooby::Owner* nested=dynamic_cast<Scrooby::Owner*>(child);
            if(nested){text=FirstText(nested);if(text)return text;}
        }
        return NULL;
    }

    void Header(CGuiScreen* screen,tFont* font)
    {
        const bool tv=Television(screen);
        const float x=tv?46:12,y=tv?51:23,size=tv?16:18,width=tv?228:296;
        Scrooby::Text* title=FindText(screen->GetScroobyScreen(),"Title");
        if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_SCRAP_BOOK_CONTENTS)title=FindText(screen->GetScroobyScreen(),"Level");
        if(!title)title=FindText(screen->GetScroobyScreen(),"Paused");
        if(!title)title=FindText(screen->GetScroobyScreen(),"PauseMenu");
        if(!title)title=FindText(screen->GetScroobyScreen(),"PhoneBooth");
        if(title && title->GetStringBuffer())BottomLabel(font,title->GetStringBuffer(),x,y,size,Gold,PauseGrid(screen)?220:width);
        else if(PauseGrid(screen))BottomLabel(font,GetTextBibleString("PAUSED"),x,y,size,Gold,220);
        else if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_OPTIONS)BottomLabel(font,GetTextBibleString("OPTIONS_(PAUSE)"),x,y,size,Gold,width);
        else if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_MISSION_SELECT)BottomLabel(font,GetTextBibleString("MISSION_SELECT"),12,23,17,Gold,200);
        else if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_SAVE_GAME)BottomLabel(font,GetTextBibleString("SAVE_GAME"),x,y,size,Gold,width);
        else if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_VIEW_CARDS || screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_CARD_GALLERY)BottomLabel(font,GetTextBibleString("VIEW_CARDS"),x,y,size,Gold,width);
        else if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_DISPLAY)Ascii(font,static_cast<CGuiScreenCheats*>(screen)->IsGraphicsPage()?"GRAPHICS":"CHEATS",x,y,size,Gold,width);
        else if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_SOUND)BottomLabel(font,GetTextBibleString("SOUND"),x,y,size,Gold,width);
        else if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_SETTINGS)BottomLabel(font,GetTextBibleString("SETTINGS"),x,y,size,Gold,width);
        else if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PHONE_BOOTH || screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PURCHASE_REWARDS)Ascii(font,screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PHONE_BOOTH?"PHONE BOOTH":"SHOP",12,23,18,Gold,222);
        if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_SCRAP_BOOK_CONTENTS){
            DirectionIcon(false,tv?228:260,tv?44:17);DirectionIcon(true,tv?260:300,tv?44:17);
        }
        if(PauseGrid(screen) || screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PHONE_BOOTH ||
           screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PURCHASE_REWARDS){
            char bank[24];sprintf(bank,"%d",GetCoinManager()->GetBankValue());
            PixelArt(UIAsset("coins"),258,16,22,19);
            Ascii(font,bank,276,23,16,White,36);
        }
    }

    struct Pickup {u64 tick;int amount;};
    Pickup s_pickups[8]={};
    unsigned s_nextPickup=0;
    int s_pickupDelta=0;
    u64 s_pickupTick=0;

    void Coin(tFont* font,float x,float y,float radius=9)
    {
        tSprite* art=UIAsset("coins");
        if(art){PixelArt(art,x,y,22,19);return;}
        Disc(x,y,radius,tColour(171,112,22));Disc(x,y,radius-2,Gold);
        if(font)Ascii(font,"$",x-radius*0.35f,y+radius*0.55f,radius*1.4f,Ink,16);
    }

    bool Question(CGuiScreen* screen)
    {
        const auto id=screen->GetWindowID();
        return id==CGuiWindow::GUI_SCREEN_ID_GENERIC_PROMPT || id==CGuiWindow::GUI_SCREEN_ID_ERROR_PROMPT ||
            id==CGuiWindow::GUI_SCREEN_ID_MISSION_OVER;
    }
    int NativeMenuTop(CGuiScreen* screen,int count)
    {
        if(Television(screen))return Question(screen)?184-count*picaBottomFrame::TVRowHeight:picaBottomFrame::TVMenuTop(count);
        return Question(screen) ? 208-count*picaBottomFrame::MenuRowHeight : picaBottomFrame::MenuTop(count);
    }

    picaBottomFrame::Rect NativeMenuItem(CGuiScreen* screen,int row,int count)
    {
        const bool tv=Television(screen),question=Question(screen);
        if(!tv && !question)return picaBottomFrame::MenuItem(row,count);
        if(tv && !question)return picaBottomFrame::TVMenuItem(row,count);
        const int top=NativeMenuTop(screen,count)+row*(tv?picaBottomFrame::TVRowHeight:picaBottomFrame::MenuRowHeight);
        return {tv?44:12,top,tv?276:308,top+(tv?24:26)};
    }

    bool Briefing(CGuiScreen* screen)
    {
        return screen && (screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_MISSION_LOAD ||
            screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_MISSION_SUCCESS);
    }

    bool Rewards(CGuiScreen* screen)
    {
        IGuiScreenRewards* preview=dynamic_cast<IGuiScreenRewards*>(screen);
        return preview && preview->HasMenu()==preview->GetPreviewMenu3DS();
    }

    // Fit the painted pixels, not the padded export canvas. The table contains
    // only retail alpha bounds; no textures or geometry are allocated per frame.
    void ObjectiveArt(Scrooby::Sprite* element,float cx=33,float cy=210,float slot=40)
    {
        tSprite* sprite=element?const_cast<tSprite*>(element->GetRawSprite()):NULL;
        if(!sprite || !element->IsVisible())return;
        static tUID previous=0;static const HandheldIconBounds::Entry* bounds=NULL;
        if(previous!=sprite->GetUID()){
            previous=sprite->GetUID();bounds=NULL;
            for(const auto& icon:HandheldIconBounds::Icons){
                char exported[64];snprintf(exported,sizeof(exported),"%s.png",icon.name);
                if(previous==tEntity::MakeUID(icon.name) || previous==tEntity::MakeUID(exported)){bounds=&icon;break;}
            }
        }
        if(bounds){
            const float scale=slot/fmaxf(float(bounds->right-bounds->left),float(bounds->bottom-bounds->top));
            const float w=bounds->width*scale,h=bounds->height*scale;
            const float x=cx+(bounds->width-bounds->left-bounds->right)*scale*0.5f;
            const float y=cy+(bounds->height-bounds->top-bounds->bottom)*scale*0.5f;
            RetailArt(sprite,x,y,w,h);
        }else{
            const float scale=slot/fmaxf(float(sprite->GetWidth()),float(sprite->GetHeight()));
            RetailArt(sprite,cx,cy,sprite->GetWidth()*scale,sprite->GetHeight()*scale);
        }
    }

    void Art(Scrooby::Sprite* element,float x,float y,float w,float h)
    {
        // Scrooby resolves untouched sprites on their first Display(). Native
        // controls bypass that method, so preserve its default-image resolution.
        if(element && !element->GetRawSprite())element->SetIndex(0);
        RetailArt(element?const_cast<tSprite*>(element->GetRawSprite()):NULL,x,y,w,h);
    }

    void FooterAction(tFont* font,const char* key,const char* caption,float left,float right)
    {
        P3D_UNICODE text[48];p3d::AsciiToUnicode(caption,text,48);
        const float badge=strcmp(key,"START")==0?40:18;
        const float captionWidth=font->GetTextWidth(text)*12/font->GetFontHeight();
        const float width=badge+6+fminf(captionWidth,right-left-badge-6);
        const float x=floorf((left+right-width)*0.5f+0.5f);
        Rounded(x,217,x+badge,235,tColour(86,78,112),tColour(35,31,54));
        P3D_UNICODE button[12];p3d::AsciiToUnicode(key,button,12);
        const float keyHeight=badge>18?10:12;
        const float keyWidth=font->GetTextWidth(button)*keyHeight/font->GetFontHeight();
        BottomLabel(font,button,x+(badge-keyWidth)*0.5f,230,keyHeight,Gold,badge-4);
        BottomLabel(font,text,x+badge+6,231,12,White,right-x-badge-6);
    }

    void RewardPanel(CGuiScreen* screen,CGuiMenu* menu)
    {
        Scrooby::Screen* resource=screen->GetScroobyScreen();
        Scrooby::Page* page=resource->GetPage(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PHONE_BOOTH?"PhoneBooth":"Rewards");
        if(!page || !menu || !menu->GetNumItems())return;
        Scrooby::Text* name=MenuLabel(menu->GetMenuItem(0));
        tFont* font=name ? name->GetFont() : NULL;if(!font)return;
        Header(screen,font);
        // Actual garage selection, never a made-up vehicle catalogue.
        Tile({10,40,310,83},true,menu->IsMenuItemEnabled(0),menu->HasSelectionBeenMade());
        BottomLabel(font,name->GetStringBuffer(),39,67,18,Ink,244);
        tSprite* left=UIAsset("Larrow0.png");tSprite* right=UIAsset("Rarrow0.png");
        if(left)RetailArt(left,24,61,24,24);else Ascii(font,"<",17,68,21,Ink,17);
        if(right)RetailArt(right,296,61,24,24);else Ascii(font,">",292,68,21,Ink,17);
        const char* prefixes[]={"Spe","Acc","Tou","Sta"};
        const char* labels[]={"Speed","Acceleration","Toughness","Handling"};
        Scrooby::Group* stats=page->GetGroup("VehicleStats");
        Scrooby::Group* locked=page->GetGroup("Locked");
        if(stats && stats->IsVisible() && (!locked || !locked->IsVisible()))for(unsigned row=0;row<4;++row){
            const float y=100+row*23;
            Scrooby::Text* caption=page->GetText(labels[row]);
            if(!caption && row==3)caption=page->GetText("Stability");
            if(caption)BottomLabel(font,caption->GetStringBuffer(),16,y+12,14,White,124);
            for(unsigned star=0;star<5;++star){char key[24];sprintf(key,"%s_Stars%u",prefixes[row],star);
                Scrooby::Sprite* image=page->GetSprite(key);
                // Reuse the retail empty / half / full stars and live rating state.
                Art(image,158+star*29,y+6,20,20);
            }
        }
        if(locked && locked->IsVisible()){
            Scrooby::Text* caption=locked->GetText("ToPurchase");
            Scrooby::Text* price=locked->GetText("Price");
            Scrooby::Text* level=page->GetText("Level");
            if(level && level->IsVisible())Wrapped(font,level->GetStringBuffer(),16,96,288,16,2,White);
            if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PHONE_BOOTH)
                Art(locked->GetSprite("Lock"),160,156,42,51);
            if(caption)Wrapped(font,caption->GetStringBuffer(),16,140,220,16,2,White);
            if(price)BottomLabel(font,price->GetStringBuffer(),238,162,18,Gold,66);
        }
        if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PURCHASE_REWARDS && locked && !locked->IsVisible()){
            Scrooby::Text* caption=locked->GetText("ToPurchase");Scrooby::Text* price=locked->GetText("Price");
            if(caption)BottomLabel(font,caption->GetStringBuffer(),16,203,13,White,196);
            if(price)BottomLabel(font,price->GetStringBuffer(),238,203,16,Gold,66);
        }
        Scrooby::Text* damaged=page->GetText("VehicleDamaged_Value");
        Scrooby::Group* damagedInfo=page->GetGroup("DamagedInfo");
        if(damaged && damagedInfo && damagedInfo->IsVisible()){
            Scrooby::Text* caption=page->GetText("VehicleDamaged");
            if(caption)BottomLabel(font,caption->GetStringBuffer(),16,197,12,White,230);
            BottomLabel(font,damaged->GetStringBuffer(),254,197,13,Gold,50);
        }
        Scrooby::Text* cost=page->GetText("CostToFix_Value");
        Scrooby::Group* repair=page->GetGroup("RepairCostInfo");
        if(cost && repair && repair->IsVisible()){
            Scrooby::Text* caption=page->GetText("CostToFix");
            if(caption)BottomLabel(font,caption->GetStringBuffer(),16,211,12,White,230);
            BottomLabel(font,cost->GetStringBuffer(),254,211,13,Gold,50);
        }
        const bool accept=screen->IsButtonVisible(BUTTON_ICON_ACCEPT);
        if(accept)FooterAction(font,"A",screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PHONE_BOOTH?
            (repair && repair->IsVisible()?"REPAIR & CALL":"CALL CAR"):"BUY",12,218);
        FooterAction(font,"B","BACK",238,310);
    }

    Scrooby::Group* ConversationButtons(CGuiScreen* screen)
    {
        if(!screen || screen->GetWindowID()!=CGuiWindow::GUI_SCREEN_ID_LETTER_BOX)return NULL;
        Scrooby::Page* page=screen->GetScroobyScreen()->GetPage("LetterBoxButtons");
        Scrooby::Group* buttons=page?page->GetGroup("Skip"):NULL;
        return buttons && buttons->IsVisible()?buttons:NULL;
    }

    void ConversationPanel(CGuiScreen* screen)
    {
        Scrooby::Group* buttons=ConversationButtons(screen);if(!buttons)return;
        Scrooby::Text* accept=buttons->GetText(screen->IsButtonVisible(BUTTON_ICON_ACCEPT)?"Accept":"Skip");
        Scrooby::Text* cancel=buttons->GetText("Cancel");
        tFont* font=accept?accept->GetFont():NULL;if(!font)return;
        CGuiScreenHud* hud=GetCurrentHud();
        Scrooby::Text* message=hud?FindText(hud->GetScroobyScreen(),"Message"):NULL;
        Mission* mission=GetGameplayManager()->GetCurrentMission();
        MissionStage* stage=mission?mission->GetCurrentStage():NULL;
        const int index=stage?stage->GetStartMessageIndex():-1;
        if(message)Wrapped(font,index>=0 && index<message->GetNumOfStrings()?
            message->GetStringBuffer(index):message->GetStringBuffer(),16,20,288,18,2,Gold);
        Tile({16,86,304,132},true,true);
        BottomLabel(font,accept->GetStringBuffer(),32,116,18,Ink,256);
        if(cancel && screen->IsButtonVisible(BUTTON_ICON_BACK)){
            Tile({16,146,304,186},false,true);
            BottomLabel(font,cancel->GetStringBuffer(),32,173,17,White,256);
        }
    }

    void BriefingPanel(CGuiScreen* screen)
    {
        Scrooby::Screen* resource=screen->GetScroobyScreen();
        Scrooby::Text* title=FindText(resource,"MissionTitle");
        Scrooby::Text* info=FindText(resource,"MissionInfo");
        tFont* font=info ? info->GetFont() : title ? title->GetFont() : NULL;
        if(!font)return;
        if(title)BottomLabel(font,title->GetStringBuffer(),12,23,18,Gold,296);
        if(info)Wrapped(font,info->GetStringBuffer(),18,48,284,17,8,White);
        Scrooby::Text* status=FindText(resource,"Loading");
        const bool loading = screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_MISSION_LOAD ?
            !static_cast<CGuiScreenMissionLoad*>(screen)->IsLoadDone() : status && status->IsVisible();
        if(loading)
        {
            // Loading artwork blinks. Its visibility does not mean the mission is ready.
            if(status)BottomLabel(font,status->GetStringBuffer(),12,232,14,White,296);
            else Ascii(font,"LOADING",12,232,14,White,296);
        }
        else {
            const bool success=screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_MISSION_SUCCESS;
            if(screen->IsButtonVisible(BUTTON_ICON_ACCEPT))FooterAction(font,"A",success?"CONTINUE":"START MISSION",12,218);
            if(screen->IsButtonVisible(BUTTON_ICON_BACK))FooterAction(font,"B","CANCEL",228,312);
        }
    }

    // BeginRender replaces the camera, matrix and current view. Every native
    // pass must preserve the caller before BeginRender, including idle FE scenes.
    class NativeViewPass
    {
        tView* view; tView* previous;
    public:
        explicit NativeViewPass(tView* target):view(target),previous(p3d::context->GetView())
        {
            p3d::pddi->PushState(PDDI_STATE_RENDER);p3d::pddi->PushState(PDDI_STATE_VIEW);
            p3d::stack->Push();
            const unsigned clear=view->GetClearMask();view->SetClearMask(0);
            view->BeginRender();view->SetClearMask(clear);
        }
        void End()
        {
            if(!view)return;
            view->EndRender();p3d::context->SetView(previous);p3d::stack->Pop();
            p3d::pddi->PopState(PDDI_STATE_VIEW);p3d::pddi->PopState(PDDI_STATE_RENDER);view=NULL;
        }
        ~NativeViewPass(){End();}
    };

    void DrawUpperVehicle(tView* view,CGuiScreenPhoneBooth* screen)
    {
        CGuiMenu* menu=screen->GetPreviewMenu3DS();
        Scrooby::Text* name=menu && menu->GetNumItems()?MenuLabel(menu->GetMenuItem(0)):NULL;
        tFont* font=name?name->GetFont():NULL;
        if(!font)return;
        p3d::pddi->PushState(PDDI_STATE_LIGHTING);
        NativeViewPass nativePass(view);
        p3d::pddi->EnableZBuffer(false);p3d::pddi->SetCullMode(PDDI_CULL_NONE);
        p3d::pddi->SetCamera(0.1f,10,1.5708f,4.0f/3);p3d::pddi->SetProjectionMode(PDDI_PROJECTION_PERSPECTIVE);
        p3d::stack->LoadIdentity();p3d::stack->Translate(-0.5f,-0.375f,0.5f);
        BottomLabel(font,name->GetStringBuffer(),24,35,20,Gold,272);
        tDrawable* model=screen->GetVehiclePreview3DS();
        if(model){
            FooterAction(font,"L","ROTATE LEFT",12,156);
            FooterAction(font,"R","ROTATE RIGHT",164,310);
            FlushUI();
            rmt::Sphere sphere;model->GetBoundingSphere(&sphere);
            if(sphere.radius>0.0001f && sphere.radius<1000){
                // The preview owns only a retail menu drawable, never vehicle
                // physics, drivers, or the selected car's gameplay resources.
                p3d::pddi->Clear(PDDI_BUFFER_DEPTH);
                p3d::pddi->EnableZBuffer(true);p3d::pddi->SetZWrite(true);
                p3d::pddi->EnableFog(false);
                p3d::pddi->SetZCompare(PDDI_COMPARE_LESS);
                p3d::pddi->SetCullMode(PDDI_CULL_NORMAL);
                p3d::pddi->SetCamera(.1f,20,1.0f,400.0f/240.0f);
                for(int light=0;light<p3d::pddi->GetMaxLights();++light)p3d::pddi->EnableLight(light,false);
                p3d::pddi->SetAmbientLight(tColour(140,140,140));
                p3d::pddi->SetLightType(0,PDDI_LIGHT_DIRECTIONAL);
                p3d::pddi->SetLightColour(0,tColour(200,200,200));
                rmt::Vector direction(-.3f,-.8f,-.5f);direction.Normalize();
                p3d::pddi->SetLightDirection(0,&direction);p3d::pddi->EnableLight(0,true);
                p3d::stack->LoadIdentity();p3d::stack->Translate(0,0,VehiclePreview::Distance);
                p3d::stack->RotateX(VehiclePreview::Pitch);p3d::stack->RotateY(screen->GetPreviewYaw3DS());
                p3d::stack->Scale(screen->GetPreviewRadius3DS()/sphere.radius);
                p3d::stack->Translate(-sphere.centre.x,-sphere.centre.y,-sphere.centre.z);
#ifdef SRR2_OLD_3DS
                const bool stereoModel=picaStereoObjectBegin(VehiclePreview::ScreenDistance);
#endif
                model->Display();
#ifdef SRR2_OLD_3DS
                if(stereoModel)picaStereoObjectEnd();
#endif
            }
        }else{
            const bool loading = screen->IsVehiclePreviewLoading3DS();
            Ascii(font,loading?"LOADING":"PREVIEW UNAVAILABLE",loading?119:48,136,16,White,224);
        }
        FlushUI();nativePass.End();
        p3d::pddi->PopState(PDDI_STATE_LIGHTING);
    }

    void DrawUpperPreview(tView* view,CGuiScreen* screen)
    {
        if(!view || !screen || !screen->IsRunning() || !screen->HasMenu())return;
        CGuiMenu* menu=screen->HasMenu();
        const int selection=menu->GetSelection();if(selection<0)return;
        Scrooby::Sprite* image=NULL;
        const bool phone=screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_PHONE_BOOTH;
        if(phone){DrawUpperVehicle(view,static_cast<CGuiScreenPhoneBooth*>(screen));return;}
        if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_SCRAP_BOOK_CONTENTS){
            const char* names[]={"StoryMissions","CharacterClothing","Vehicles",NULL,"CollectorCards"};
            Scrooby::Page* page=screen->GetScroobyScreen()->GetPage("ScrapBookContents");
            if(page && selection<5 && names[selection])image=page->GetSprite(names[selection]);
        }else{
            int rows[64];if(MenuSprites(menu,rows)>0)
                image=dynamic_cast<Scrooby::Sprite*>(menu->GetMenuItem(selection)->GetItem());
        }
        tSprite* art=image?const_cast<tSprite*>(image->GetRawSprite()):NULL;
        if(!art || art->GetWidth()<=8 || art->GetHeight()<=8)return;
        float scale=150.0f/art->GetHeight();
        const float maxWidth=254.0f;
        if(art->GetWidth()*scale>maxWidth)scale=maxWidth/art->GetWidth();
        // The upper display presents the focused retail artwork. Its source
        // sprite already follows the selected level and unlock state.
        NativeViewPass nativePass(view);
        p3d::pddi->EnableZBuffer(false);p3d::pddi->SetCullMode(PDDI_CULL_NONE);
        p3d::pddi->SetCamera(0.1f,10,1.5708f,4.0f/3);p3d::pddi->SetProjectionMode(PDDI_PROJECTION_PERSPECTIVE);
        p3d::stack->LoadIdentity();p3d::stack->Translate(-0.5f,-0.375f,0.5f);
        // 320-wide Scrooby space fills a 400-wide LCD; compensate to retain art proportions.
        RetailArt(art,160,133,art->GetWidth()*scale*0.8f,art->GetHeight()*scale);
        nativePass.End();
    }

    void DrawUpperCredits(tView* view,CGuiScreen* screen)
    {
        if(!view || !screen || !screen->IsRunning() || screen->GetWindowID()!=CGuiWindow::GUI_SCREEN_ID_VIEW_CREDITS)return;
        CGuiScreenViewCredits* credits=static_cast<CGuiScreenViewCredits*>(screen);
        Scrooby::Text* first=credits->GetCreditsText3DS(0);
        tFont* font=first?first->GetFont():NULL;
        if(!font)return;
        NativeViewPass nativePass(view);
        p3d::pddi->EnableZBuffer(false);p3d::pddi->SetCullMode(PDDI_CULL_NONE);
        p3d::pddi->SetCamera(0.1f,10,1.5708f,4.0f/3);p3d::pddi->SetProjectionMode(PDDI_PROJECTION_PERSPECTIVE);
        p3d::stack->LoadIdentity();p3d::stack->Translate(-0.4f,-0.375f,0.5f);
        p3d::stack->Scale(0.8f,1,1); // Square text pixels on the 400-wide LCD.
        if(credits->IsPortThanks3DS())
        {
            Gradient(-40,0,360,240,tColour(49,41,79),tColour(14,15,35));
            auto centered=[&](const char* text,float y,float height,tColour colour){
                P3D_UNICODE label[128];p3d::AsciiToUnicode(text,label,128);
                const float width=fminf(288,font->GetTextWidth(label)*height/font->GetFontHeight());
                BottomLabel(font,label,(320-width)*0.5f,y,height,colour,288);
            };
            centered("THANK YOU",65,22,Gold);
            centered("Thank you for playing this port.",104,15,White);
            centered("Impossible, until someone does it.",132,14,White);
            centered("AlexMolina08",179,16,Gold);
            centered("23 October 2026",205,12,White);
            nativePass.End();
            return;
        }
        const float scroll=credits->GetCreditsScroll3DS();
        auto line=[&](tFont* face,const P3D_UNICODE* label,float baseline,tColour colour){
            const float y=baseline-scroll;
            if(y<20 || y>224)return;
            const float fade=fminf(1.0f,fminf((y-20)/12,(224-y)/12));
            colour.SetAlpha(int(fmaxf(0.0f,fade)*255));
            const float width=face->GetTextWidth(label)*HandheldCredits::Height/face->GetFontHeight();
            BottomLabel(face,label,(320-width)*0.5f,y,HandheldCredits::Height,colour,HandheldCredits::Width);
        };
        P3D_UNICODE role[64],name[32];
        p3d::AsciiToUnicode("3DS adaptation made by",role,64);p3d::AsciiToUnicode("AlexMolina08",name,32);
        line(font,role,HandheldCredits::RoleY,Gold);line(font,name,HandheldCredits::NameY,Gold);
        float baseline=HandheldCredits::OriginalY;
        for(unsigned i=0;i<credits->GetNumCreditsTexts3DS();++i){
            Scrooby::Text* source=credits->GetCreditsText3DS(i);
            if(!source || !source->IsVisible() || !source->GetFont())continue;
            tFont* face=source->GetFont();
            const unsigned rows=HandheldCredits::Walk(
                static_cast<const P3D_UNICODE*>(source->GetStringBuffer()),
                [face](const P3D_UNICODE* text){return face->GetTextWidth(text)*HandheldCredits::Height/face->GetFontHeight()<=HandheldCredits::Width;},
                [&](const P3D_UNICODE* text,unsigned row){line(face,text,baseline+row*HandheldCredits::Advance,White);});
            baseline+=rows*HandheldCredits::Advance+HandheldCredits::PartitionGap;
        }
        nativePass.End();
    }

    void DrawUpperObjective(tView* view,CGuiScreen* screen)
    {
        if(!view || !screen || !screen->IsRunning() || !PauseGrid(screen))return;
        Scrooby::Page* page=screen->GetScroobyScreen()->GetPage("PauseMissionObjective");
        Scrooby::Group* group=page?page->GetGroup("MissionObjective"):NULL;
        Scrooby::Text* label=group?group->GetText("MissionObjective"):NULL;
        if(!group || !group->IsVisible() || !label || !label->IsVisible() || !label->GetFont())return;
        const P3D_UNICODE* text=label->GetStringBuffer();
        if(!text || !text[0])return;
        tFont* font=label->GetFont();
        struct Layout {tFont* font=NULL;P3D_UNICODE text[512]={};unsigned lines=1;float size=16,width=0;};
        static Layout layout;
        unsigned n=0;while(n<511 && text[n] && text[n]==layout.text[n])++n;
        if(layout.font!=font || n==511 || text[n]!=layout.text[n]){
            layout.font=font;n=0;while(n<511 && text[n]){layout.text[n]=text[n];++n;}layout.text[n]=0;
            P3D_UNICODE lines[16][256];
            for(float size=16;size>=12;--size){
                layout.size=size;
                layout.lines=HandheldText::Wrap(layout.text,lines,[&](const P3D_UNICODE* line){
                    return font->GetTextWidth(line)*size/font->GetFontHeight()<=272;
                });
                if(layout.lines<=7)break;
            }
            layout.width=font->GetTextWidth(layout.text)*layout.size/font->GetFontHeight();
        }
        Scrooby::Sprite* portrait=group->GetSprite("ObjectiveIcon");
        const bool hasPortrait=portrait && portrait->IsVisible() && portrait->GetRawSprite();
        const unsigned lines=layout.lines<7?layout.lines:7;
        const float height=fmaxf(68,24+lines*layout.size+(lines-1)*3);
        const float top=120-height*0.5f;
        NativeViewPass nativePass(view);
        p3d::pddi->EnableZBuffer(false);p3d::pddi->SetCullMode(PDDI_CULL_NONE);
        p3d::pddi->SetCamera(0.1f,10,1.5708f,4.0f/3);p3d::pddi->SetProjectionMode(PDDI_PROJECTION_PERSPECTIVE);
        p3d::stack->LoadIdentity();p3d::stack->Translate(-0.5f,-0.375f,0.5f);
        // Upper LCD is 400 pixels wide. Keep lettering and portraits square.
        p3d::stack->Scale(0.8f,1,1);
#ifdef SRR2_OLD_3DS
        const bool stereoCard = picaStereoObjectBegin(0.95f);
#endif
        const float width=lines==1?fmaxf(240,layout.width+(hasPortrait?88:32)):360;
        const float left=(400-width)*0.5f;
        Rounded(left,top,left+width,top+height,tColour(62,53,95,200),tColour(26,25,49,212));
        RetailFrame(left,top,left+width,top+height,true);
        if(lines==1){
            const float content=(400-layout.width-(hasPortrait?60:0))*0.5f;
            if(hasPortrait)ObjectiveArt(portrait,content+24,120,48);
            BottomLabel(font,text,content+(hasPortrait?60:0),120-layout.size*0.5f+
                layout.size*font->GetFontBaseLine()/font->GetFontHeight(),layout.size,Gold,272);
        }else{
            if(hasPortrait)ObjectiveArt(portrait,60,120,48);
            Wrapped(font,text,hasPortrait?96:64,top+12,272,16,7,Gold,height-24);
        }
        FlushUI();
#ifdef SRR2_OLD_3DS
        if(stereoCard) picaStereoObjectEnd();
#endif
        nativePass.End();
    }

    void DrawUpperStats(tView* view,CGuiScreen* screen)
    {
        if(!view || !screen || !screen->IsRunning() || screen->GetWindowID()!=CGuiWindow::GUI_SCREEN_ID_SCRAP_BOOK_STATS)return;
        Scrooby::Text* label=FindText(screen->GetScroobyScreen(),"GameComplete");
        Scrooby::Text* value=FindText(screen->GetScroobyScreen(),"GameComplete_Value");
        tFont* font=label?label->GetFont():NULL;if(!font || !value)return;
        NativeViewPass nativePass(view);
        p3d::pddi->EnableZBuffer(false);p3d::pddi->SetCullMode(PDDI_CULL_NONE);
        p3d::pddi->SetCamera(0.1f,10,1.5708f,4.0f/3);p3d::pddi->SetProjectionMode(PDDI_PROJECTION_PERSPECTIVE);
        p3d::stack->LoadIdentity();p3d::stack->Translate(-0.5f,-0.375f,0.5f);
        float width=font->GetTextWidth(label->GetStringBuffer())*18/font->GetFontHeight();
        BottomLabel(font,label->GetStringBuffer(),(320-width)*0.5f,102,18,White,272);
        width=font->GetTextWidth(value->GetStringBuffer())*42/font->GetFontHeight();
        BottomLabel(font,value->GetStringBuffer(),(320-width)*0.5f,159,42,Gold,272);
        nativePass.End();
    }

    void DrawHUD(tView* view)
    {
        CGuiScreenHud* hud=GetCurrentHud();
        if(!view || !hud)return;
        Scrooby::Page* page=hud->GetScroobyScreen()->GetPage("Hud");
        Scrooby::Text* message=page ? page->GetText("Message") : NULL;
        tFont* font=message ? message->GetFont() : NULL;
        Scrooby::Layer* layer=page?page->GetLayer("Foreground"):NULL;
        if(!font || (layer && !layer->IsVisible())){
            if(picaBottomScreenInterfaceBegin())picaBottomScreenEnd();return;
        }
        if(!picaBottomScreenInterfaceBegin())return;
        NativeViewPass nativePass(view);
        p3d::pddi->EnableZBuffer(false);p3d::pddi->SetCullMode(PDDI_CULL_NONE);
        p3d::pddi->SetCamera(0.1f,10,1.5708f,4.0f/3);p3d::pddi->SetProjectionMode(PDDI_PROJECTION_PERSPECTIVE);
        p3d::stack->LoadIdentity();p3d::stack->Translate(-0.5f,-0.375f,0.5f);
        Mission* mission=GetGameplayManager()->GetCurrentMission();
        MissionStage* stage=mission ? mission->GetCurrentStage() : NULL;
        const int objective=stage ? stage->GetStartMessageIndex() : -1;
        const P3D_UNICODE* text=objective>=0 && objective<message->GetNumOfStrings()
            ? message->GetStringBuffer(objective) : NULL;
        Scrooby::Group* help=page->GetGroup("Message");
        if(help && help->IsVisible() && help->GetAlpha()>0.5f)text=message->GetStringBuffer();
        const bool objectiveShown=text && text[0];
        Board(false,false);
        FlushUI();
        // The original radar, road geometry, icons and Hit & Run meter remain intact.
        if(s_bottomMap && s_mapWasVisible){
            float source[4];s_bottomMap->GetBottomScreenSource(source);
            const auto radar=objectiveShown?picaBottomFrame::Radar:picaBottomFrame::IdleRadar;
            picaBottomScreenMapping(source,radar.left,radar.top,radar.right,radar.bottom);
            p3d::stack->Push();
            s_bottomMap->DisplayOnBottomScreen(view,s_backgroundWasVisible?s_bottomBackground:NULL);
            p3d::stack->Pop();
            const float full[4]={0,0,1,1};picaBottomScreenMapping(full,0,0,320,240);
        }
        // Mask the map's external artwork away from the information rows.
        Rounded(7,4,313,32,tColour(62,53,95),tColour(26,25,49));
        if(objectiveShown){Rounded(7,184,313,237,tColour(62,53,95),tColour(26,25,49));
            RetailFrame(7,184,313,237,true);}
        char progress[32],bank[32];
        const RenderEnums::LevelEnum level=GetGameplayManager()->GetCurrentLevelIndex();
        sprintf(progress,"%d",GetCharacterSheetManager()->QueryNumWaspsDestroyed(level));
        PixelArt(UIAsset("camicon"),28,18,30,20.25f);
        Ascii(font,progress,50,23,17,Gold,160);
        const u64 now=svcGetSystemTick();
        const float age=s_pickupTick ? float(now-s_pickupTick)/SYSCLOCK_ARM11 : 10;
        const float pop=age<0.24f ? sinf(age*13.089969f)*2 : 0;
        Coin(font,252,16-floorf(pop+0.5f));
        sprintf(bank,"%d",GetCoinManager()->GetBankValue());
        Ascii(font,bank,268,23,17,White,45);
        for(const auto& pickup:s_pickups)if(pickup.tick){
            const float t=float(now-pickup.tick)/SYSCLOCK_ARM11/0.55f;
            if(t>=0 && t<1){const float ease=1-(1-t)*(1-t);
                Coin(font,210+(252-210)*ease,126+(16-126)*ease,6);}
        }
        if(age<1.2f && s_pickupDelta){
            char change[24];sprintf(change,"%+d",s_pickupDelta);
            Ascii(font,change,270,48,14,s_pickupDelta>0?Gold:tColour(255,119,88),44);
        }
        Scrooby::Group* action=page->GetGroup("ActionButton");
        const bool actionable=action && action->IsVisible();
        Scrooby::Sprite* portrait=page->GetSprite("ObjectiveIcon");
        const bool hasPortrait=portrait && portrait->IsVisible() && portrait->GetRawSprite();
        if(objectiveShown && hasPortrait)ObjectiveArt(portrait);
        if(actionable){
            // Interaction and mission objective are distinct. Standing beside
            // a car must never make the X badge imply "Talk to Marge".
            Avatar* avatar=GetAvatarManager()->GetAvatarForPlayer(0);
            Character* character=avatar?avatar->GetCharacter():NULL;
            ActionButton::ButtonHandler* handler=character?character->GetActionButtonHandler():NULL;
            const char* caption="INTERACT";
            if(handler)switch(handler->GetType()){
                case ActionButton::ButtonHandler::MISSION_OBJECTIVE:caption="TALK";break;
                case ActionButton::ButtonHandler::GET_IN_USER_CAR:
                case ActionButton::ButtonHandler::GET_IN_CAR:caption="ENTER VEHICLE";break;
                case ActionButton::ButtonHandler::SUMMON_PHONE:caption="PHONE BOOTH";break;
                case ActionButton::ButtonHandler::INTERIOR:caption="GO INSIDE";break;
                case ActionButton::ButtonHandler::PURCHASE_CAR:caption="BUY VEHICLE";break;
                case ActionButton::ButtonHandler::PURCHASE_SKIN:caption="OUTFITS";break;
                default:break;
            }
            // The whole framed control is one touch target. Keep its hardware
            // hint and actual nearby action together, separate from the mission.
            const auto bounds=picaBottomFrame::Interaction;
            const float press=s_hudActionPressed?1:0;
            Rounded(bounds.left,bounds.top+2,bounds.right,bounds.bottom,tColour(9,8,23),tColour(9,8,23));
            Rounded(bounds.left,bounds.top+press,bounds.right,bounds.bottom-2+press,
                s_hudActionPressed?tColour(255,239,123):tColour(82,70,115),
                s_hudActionPressed?tColour(222,168,36):tColour(38,31,64));
            RetailFrame(bounds.left,bounds.top+press,bounds.right,bounds.bottom-2+press,s_hudActionPressed);
            Rounded(28,124+press,52,146+press,tColour(11,10,23),tColour(11,10,23));
            Rounded(29,124+press,51,143+press,tColour(130,126,145),tColour(53,50,67));
            Ascii(font,"X",35,138+press,13,White,14);
            P3D_UNICODE label[32];p3d::AsciiToUnicode(caption,label,32);
            // Fit compelete words rather than letting the narrow caption split glyphs across rows.
            P3D_UNICODE* second=NULL;
            for(P3D_UNICODE* c=label;*c;++c)if(*c==' '){*c=0;second=c+1;break;}
            const unsigned rows=second?2:1;
            const float top=149+press+(27-(rows*12+(rows-1)*3))*0.5f;
            for(unsigned row=0;row<rows;++row){
                const P3D_UNICODE* word=row?second:label;
                const float naturalWidth=font->GetTextWidth(word)*12/font->GetFontHeight();
                const float height=naturalWidth>56?12*56/naturalWidth:12;
                const float width=font->GetTextWidth(word)*height/font->GetFontHeight();
                const float baseline=top+row*15+(12-height)*0.5f+height*font->GetFontBaseLine()/font->GetFontHeight();
                BottomLabel(font,word,12+(56-width)*0.5f,baseline,height,s_hudActionPressed?Ink:White,56);
            }
        }
        if(text)Wrapped(font,text,hasPortrait?64:20,190,hasPortrait?237:280,13,3,White,41);
        FlushUI();nativePass.End();picaBottomScreenEnd();
    }

    void DisplayBottomInterface( tView* view )
    {
        CGuiScreen* screen=CurrentBottomScreen();
        const CGuiWindow::eGuiWindowID id=screen ? screen->GetWindowID() : CGuiWindow::GUI_WINDOW_ID_UNDEFINED;
        const bool startupLoading=id==CGuiWindow::GUI_SCREEN_ID_LOADING_FE ||
            id==CGuiWindow::GUI_SCREEN_ID_BOOTUP_LOAD;
        const bool loading=startupLoading || id==CGuiWindow::GUI_SCREEN_ID_LOADING ||
            id==CGuiWindow::GUI_SCREEN_ID_MISSION_LOAD;
        const bool tutorial=id==CGuiWindow::GUI_SCREEN_ID_TUTORIAL;
        const bool stats=id==CGuiWindow::GUI_SCREEN_ID_LEVEL_STATS;
        const bool gameStats=id==CGuiWindow::GUI_SCREEN_ID_SCRAP_BOOK_STATS;
        const bool splash=id==CGuiWindow::GUI_SCREEN_ID_SPLASH;
        const bool credits=id==CGuiWindow::GUI_SCREEN_ID_VIEW_CREDITS;
        const bool briefing=Briefing(screen),rewards=Rewards(screen);
        const bool messageOnly=id==CGuiWindow::GUI_SCREEN_ID_GENERIC_MESSAGE;
        const bool conversation=ConversationButtons(screen)!=NULL;
        const bool gallery=id==CGuiWindow::GUI_SCREEN_ID_VIEW_CARDS || id==CGuiWindow::GUI_SCREEN_ID_CARD_GALLERY;
        const bool movie=GetPresentationManager() && GetPresentationManager()->GetFMVPlayer() &&
            GetPresentationManager()->GetFMVPlayer()->IsPlaying();
        // Cinematics and boot artwork belong to the upper display only.
        if( view == NULL || screen == NULL || movie || (!loading && !tutorial && !stats && !gameStats && !briefing && !messageOnly && !gallery && !conversation && !credits && !screen->HasMenu()) )
        {
            if( picaBottomScreenInterfaceBegin() )
            {
                picaBottomScreenEnd();
            }
            return;
        }
        const bool mainMenu=screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_MAIN_MENU;
        CGuiMenu* menu=screen->HasMenu();
        Scrooby::Text* choices=mainMenu && menu && menu->GetNumItems()>0
            ? dynamic_cast<Scrooby::Text*>(menu->GetMenuItem(0)->GetItem()) : NULL;
        const int count=choices ? menu->GetMenuItem(0)->m_itemValueCount : 0;
        const int selection=choices ? menu->GetSelectionValue(0) : -1;
        const bool list=choices && choices->GetFont() && count>0 && count<=6;
        int rows[64];const int rowCount=mainMenu ? 0 : MenuRows(menu,rows);
        int imageRows[64];const int imageCount=MenuSprites(menu,imageRows);
        const bool cardDetail=!menu && (id==CGuiWindow::GUI_SCREEN_ID_VIEW_CARDS || id==CGuiWindow::GUI_SCREEN_ID_CARD_GALLERY);
        int selectedRow=-1;
        for( int i=0;i<rowCount;++i ) if(rows[i]==menu->GetSelection())selectedRow=i;
        const int focus=mainMenu?selection:menu?menu->GetSelection():-1;
        if(s_focusScreen!=screen || s_focusSelection!=focus){s_focusScreen=screen;s_focusSelection=focus;s_focusTick=svcGetSystemTick();s_textScrollDelta=0;}
        const bool grid=PauseGrid(screen),tv=Television(screen);
        const int pageSize=tv?picaBottomFrame::TVPageSize:6;
        const int first=grid ? 0 : selectedRow>=0 ? (selectedRow/pageSize)*pageSize : 0;
        const int visibleRows=grid ? rowCount : rowCount-first<pageSize ? rowCount-first : pageSize;
        if( !picaBottomScreenInterfaceBegin() ) return;
        NativeViewPass nativePass(view);
        picaBottomScreenPanel(-1,0);
        p3d::pddi->EnableZBuffer(false);
        p3d::pddi->SetCamera(0.1f,10.0f,1.5708f,4.0f/3);
        p3d::pddi->SetProjectionMode(PDDI_PROJECTION_PERSPECTIVE);
        p3d::pddi->SetCullMode(PDDI_CULL_NONE);
        p3d::stack->LoadIdentity();
        p3d::stack->Translate(-0.5f,-0.375f,0.5f);
        if(tutorial)Fill(0,0,320,240,tColour(32,28,50));else Board(tv,!conversation && !startupLoading && id!=CGuiWindow::GUI_SCREEN_ID_LOADING);
        FlushUI();if(tv)TVClip(true);
        // Keep the tube illuminated while the camera changes menus. Only the
        // controls disappear; a rectangular blackout exposes the artwork bounds.
        if(tv && !screen->IsRunning()){}
        else if(credits)
        {
            Scrooby::Text* text=static_cast<CGuiScreenViewCredits*>(screen)->GetCreditsText3DS(0);
            tFont* font=text?text->GetFont():NULL;
            const bool thanks=static_cast<CGuiScreenViewCredits*>(screen)->IsPortThanks3DS();
            Ascii(font,thanks?"THE END":"CREDITS",46,51,16,Gold,228);
            const picaBottomFrame::Rect back={44,146,276,178};
            TVRow(back,true,false);
            Ascii(font,thanks?"MAIN MENU":"BACK",thanks?103:126,168,16,Gold,140);
            Ascii(font,thanks?"A CONTINUE":"A BACK",46,201,12,White,120);
            if(!thanks)Ascii(font,"B BACK",224,201,12,White,50);
        }
        else if( startupLoading || splash )
        {
            Scrooby::Screen* resource=screen->GetScroobyScreen();
            Scrooby::Text* label=splash && menu && menu->GetNumItems()>0
                ? MenuLabel(menu->GetMenuItem(0)) : NULL;
            for( int i=0; resource && !label && i<resource->GetNumberOfPages(); ++i )
                label=resource->GetPageByIndex(i)->GetText("Loading");
            if( label && label->GetFont() )
            {
                tFont* font=label->GetFont();
                if(startupLoading)LoadingPanel(label);
                else
                {
                    const P3D_UNICODE* text=label->GetStringBuffer();
                    const float width=font->GetFontHeight()>0
                        ? font->GetTextWidth(text)*16/font->GetFontHeight() : 0;
                    BottomLabel(font,text,(320-width)*0.5f,128,16,White);
                }
            }
        }
        else if(conversation)ConversationPanel(screen);
        else if(briefing)BriefingPanel(screen);
        else if(rewards)RewardPanel(screen,menu);
        else if(messageOnly){
            Scrooby::Text* message=FindText(screen->GetScroobyScreen(),"Message");
            if(message && message->GetFont()){
                tFont* font=message->GetFont();Wrapped(font,message->GetStringBuffer(),16,42,288,17,8,White);
                if(screen->IsButtonVisible(BUTTON_ICON_ACCEPT))FooterAction(font,"A","CONTINUE",12,164);
                if(screen->IsButtonVisible(BUTTON_ICON_BACK))FooterAction(font,"B","BACK",238,310);
            }
        }
        else if( list )
        {
            tFont* font=choices->GetFont();
            Ascii(font,"MAIN MENU",tv?46:12,tv?51:23,tv?16:18,Gold,tv?228:296);
            P3D_UNICODE hint[48];
            p3d::AsciiToUnicode("A SELECT",hint,48);
            if(screen->IsButtonVisible(BUTTON_ICON_ACCEPT))BottomLabel(font,hint,tv?46:12,tv?201:231,tv?12:13,White,tv?228:296);
            const float scroll=tv?MainScrollPosition(menu):0;
            if(tv)ClipRegion({43,58,277,188});
            for(int row=0;row<count;++row){
                const int value=picaBottomFrame::MainChoice(row,count);
                const auto rect=tv?picaBottomFrame::MainItem(row,scroll):picaBottomFrame::MenuItem(row,count);
                if(tv && (rect.bottom<=58 || rect.top>=188))continue;
                if(tv){TVRow(rect,value==selection,Pressed(menu,value));if(value==selection)DirectionIcon(true,51,(rect.top+rect.bottom)*0.5f);}
                else Tile(rect,value==selection,true,Pressed(menu,value));
                const tColour colour=value==selection && !screen->IsButtonVisible(BUTTON_ICON_ACCEPT)?tColour(151,146,168):value==selection?(tv?Gold:Ink):White;
                BottomLabel(font,choices->GetStringBuffer(value),rect.left+(tv?23:14),rect.top+(tv?18:20)+(tv?(Pressed(menu,value)?1:0):TileLift(value==selection,Pressed(menu,value))),16,colour,rect.right-rect.left-(tv?34:28));
            }
            if(tv){TVClip(true);if(count>5){
                const float thumb=126*5.0f/count,top=60+(126-thumb)*scroll/(count-5);
                Fill(273,60,275,186,tColour(115,105,142,100));Fill(273,top,275,top+thumb,Gold);
            }}
        }
        else if(cardDetail){
            Scrooby::Text* title=FindText(screen->GetScroobyScreen(),"CardTitle");
            Scrooby::Text* episode=FindText(screen->GetScroobyScreen(),"CardEpisode");
            Scrooby::Text* description=FindText(screen->GetScroobyScreen(),"CardDescription");
            tFont* font=title?title->GetFont():NULL;
            if(font){
                BottomLabel(font,title->GetStringBuffer(),12,23,18,Gold,296);
                if(episode)Wrapped(font,episode->GetStringBuffer(),16,40,288,15,2,White);
                if(description)Wrapped(font,description->GetStringBuffer(),16,82,288,15,7,White);
                FooterAction(font,"B","BACK",238,310);
            }
        }
        else if(imageCount>0){
            int selected=0;for(int i=0;i<imageCount;++i)if(imageRows[i]==menu->GetSelection())selected=i;
            const int first=(selected/8)*8,shown=imageCount-first<8?imageCount-first:8;
            Scrooby::Text* text=NULL;
            for(int i=0;i<screen->GetScroobyScreen()->GetNumberOfPages() && !text;++i)text=FirstText(screen->GetScroobyScreen()->GetPageByIndex(i));
            tFont* font=text?text->GetFont():NULL;
            if(font){Header(screen,font);
                const bool selectable=menu->GetSelection()>=0 && menu->IsMenuItemEnabled(menu->GetSelection());
                if(tv){if(selectable)Ascii(font,"A SELECT",46,201,12,White);Ascii(font,"B BACK",224,201,12,White,50);}
                else {if(selectable)FooterAction(font,"A","SELECT",12,140);FooterAction(font,"B","BACK",238,310);}
            }
            for(int i=0;i<shown;++i){
                const int index=imageRows[first+i];const auto r=tv?picaBottomFrame::TVThumbnail(i):picaBottomFrame::Thumbnail(i);
                Tile(r,first+i==selected,menu->IsMenuItemEnabled(index),Pressed(menu,index));
                Scrooby::Sprite* sprite=dynamic_cast<Scrooby::Sprite*>(menu->GetMenuItem(index)->GetItem());
                tSprite* art=sprite?const_cast<tSprite*>(sprite->GetRawSprite()):NULL;
                if(art){float w=float(art->GetWidth()),h=float(art->GetHeight()),scale=(r.right-r.left-8)/w;
                    if(h*scale>r.bottom-r.top-8)scale=(r.bottom-r.top-8)/h;
                    Art(sprite,(r.left+r.right)*0.5f,(r.top+r.bottom)*0.5f,w*scale,h*scale);}
            }
        }
        else if( visibleRows>0 )
        {
            tFont* hintFont=MenuLabel(menu->GetMenuItem(rows[first]))->GetFont();
            Header(screen,hintFont);
            if(id==CGuiWindow::GUI_SCREEN_ID_DISPLAY && static_cast<CGuiScreenCheats*>(screen)->IsGraphicsPage())
            {
                Ascii(hintFont,Graphics3DS::Description(),tv?46:16,tv?77:56,12,White,tv?228:288);
                Ascii(hintFont,Graphics3DS::Status(),tv?46:16,tv?93:73,11,White,tv?228:288);
            }
            if(Question(screen)){
                Scrooby::Text* question=FindText(screen->GetScroobyScreen(),"Message");
                Scrooby::Text* reason=FindText(screen->GetScroobyScreen(),"MissionFailureReason");
                Scrooby::Text* hint=FindText(screen->GetScroobyScreen(),"MissionFailureHint");
                if(question){const int top=tv?56:42;const int available=NativeMenuTop(screen,visibleRows)-top-4;const unsigned lines=available>0?available/19:1;
                    Wrapped(hintFont,question->GetStringBuffer(),tv?48:16,top,tv?224:288,16,lines?lines:1,White);}
                if(reason)Wrapped(hintFont,reason->GetStringBuffer(),16,42,288,16,2,White);
                if(hint)Wrapped(hintFont,hint->GetStringBuffer(),16,80,288,14,2,White);
            }
            for( int row=0;row<visibleRows;++row )
            {
                const int index=rows[first+row];
                GuiMenuItem* item=menu->GetMenuItem(index);
                Scrooby::Text* label=MenuLabel(item);
                Scrooby::Text* value=dynamic_cast<Scrooby::Text*>(item->GetItemValue());
                const bool slider=item->m_slider.m_pPolygon || item->m_slider.m_pImage;
                const bool separate=value && value!=label;
                const picaBottomFrame::Rect rect=grid ? picaBottomFrame::PauseItem(row,visibleRows) :
                    NativeMenuItem(screen,row,visibleRows);
                if(tv){TVRow(rect,first+row==selectedRow,Pressed(menu,index));
                    if(first+row==selectedRow)DirectionIcon(true,51,(rect.top+rect.bottom)*0.5f);}
                else Tile(rect,first+row==selectedRow,menu->IsMenuItemEnabled(index),Pressed(menu,index));
                const float y=rect.top+(rect.bottom-rect.top)/2+6+(tv?(Pressed(menu,index)?1:0):TileLift(first+row==selectedRow,Pressed(menu,index)));
                const tColour colour=!menu->IsMenuItemEnabled(index) ? tColour(151,146,168) : first+row==selectedRow ? (tv?Gold:Ink) : White;
                const bool single=visibleRows==1 && !separate && !slider;
                const float valueWidth=separate && value->GetFont()?value->GetFont()->GetTextWidth(value->GetStringBuffer())*14/value->GetFont()->GetFontHeight():0;
                const float labelWidth=label->GetFont()->GetTextWidth(label->GetStringBuffer())*16/label->GetFont()->GetFontHeight();
                const bool stacked=tv && separate && labelWidth+valueWidth+12>198;
                const float height=stacked?12:grid && row>0 ? 14.0f : 16.0f;
                float x=rect.left+(tv?23:14);
                if(grid){
                    // The retail pause menu is text-led. Secondary actions do
                    // not borrow unrelated controller, collectible or back icons.
                    // Centre every label in the same cell, including disabled rows.
                    const float measured=label->GetFont()->GetTextWidth(label->GetStringBuffer())*
                        height/label->GetFont()->GetFontHeight();
                    const float available=rect.right-rect.left-20;
                    x=(rect.left+rect.right-fminf(measured,available))*0.5f;
                    if(row==0){
                        tSprite* play=UIAsset("Rarrow0.png");
                        if(play){
                            const float scale=18/fmaxf(float(play->GetWidth()),float(play->GetHeight()));
                            const float left=(320-measured-28)*0.5f;
                            RetailArt(play,left+9,(rect.top+rect.bottom)*0.5f+
                                TileLift(first+row==selectedRow,Pressed(menu,index)),
                                play->GetWidth()*scale,play->GetHeight()*scale);
                            x=left+28;
                        }
                    }
                }
                if( single && label->GetFont()->GetFontHeight()>0 )
                {
                    float width=label->GetFont()->GetTextWidth(label->GetStringBuffer())*
                        height/label->GetFont()->GetFontHeight();
                    if(width>230)width=230;
                    x=(320-width)*0.5f;
                }
                const bool missionList=id==CGuiWindow::GUI_SCREEN_ID_MISSION_SELECT;
                const float valueLeft=tv?rect.right-10-valueWidth:202;
                BottomLabel(label->GetFont(),label->GetStringBuffer(),x,stacked?rect.top+12:y,height,colour,
                    grid ? rect.right-x-8 : missionList?188:tv?(separate&&!stacked?valueLeft-x-8:slider?120:rect.right-x-8):separate || slider ? 170 : 272);
                if(missionList){
                    char key[32];sprintf(key,"MissionStatus%d",index);
                    Scrooby::Page* page=screen->GetScroobyScreen()->GetPage("MissionSelect");
                    Scrooby::Sprite* status=page?page->GetSprite(key):NULL;
                    if(status && status->IsVisible())Art(status,284,(rect.top+rect.bottom)*0.5f,18,18);
                }
                if( separate && value->GetFont() )
                    BottomLabel(value->GetFont(),value->GetStringBuffer(),stacked?x:valueLeft,stacked?rect.top+24:y,stacked?12:14,colour,stacked?rect.right-x-8:tv?valueWidth:94);
                else if( slider )
                {
                    const float middle=(rect.top+rect.bottom)*0.5f+(tv?0:TileLift(first+row==selectedRow,Pressed(menu,index)));
                    const float left=tv?194:202,width=tv?72:96;
                    Fill(left,middle-2,left+width,middle+2,first+row==selectedRow?tColour(148,107,26):tColour(24,20,43));
                    Fill(left,middle-2,left+width*item->m_slider.m_value,middle+2,first+row==selectedRow && !tv?Ink:Gold);
                    const float radius=tv?4:6;
                    Disc(left+width*item->m_slider.m_value,middle+1,radius+1,tColour(10,8,22,150));
                    Disc(left+width*item->m_slider.m_value,middle,radius,White);
                }
            }
            if(id==CGuiWindow::GUI_SCREEN_ID_MISSION_SELECT){
                Scrooby::Text* level=FindText(screen->GetScroobyScreen(),"Level");
                if(level)BottomLabel(hintFont,level->GetStringBuffer(),240,23,15,Gold,48);
                DirectionIcon(false,226,17);DirectionIcon(true,305,17);
            }
            GuiMenuItem* current=menu->GetSelection()>=0?menu->GetMenuItem(menu->GetSelection()):NULL;
            const bool change=ChangesValue(menu);
            if(change){
                Rounded(tv?44:12,tv?190:215,tv?78:48,tv?207:237,tColour(85,73,119),tColour(34,30,55));
                Rounded(tv?82:52,tv?190:215,tv?116:88,tv?207:237,tColour(85,73,119),tColour(34,30,55));
                DirectionIcon(false,tv?62:30,tv?200:226);DirectionIcon(true,tv?100:70,tv?200:226);
                Ascii(hintFont,"CHANGE",tv?122:96,tv?201:232,tv?12:13,White,tv?85:120);
                if(tv)Ascii(hintFont,"B BACK",224,201,12,White,50);
                else FooterAction(hintFont,"B","BACK",238,310);
            }else {
                const bool selectable=current && (current->m_attributes&SELECTABLE);
                if(selectable){
                    if(tv)Ascii(hintFont,"A SELECT",46,201,12,White,120);
                    else FooterAction(hintFont,"A","SELECT",12,140);
                }
                if(grid)FooterAction(hintFont,"START","CONTINUE",164,312);
                else if(!selectable || screen->IsButtonVisible(BUTTON_ICON_BACK)){
                    if(tv)Ascii(hintFont,"B BACK",224,201,12,White,50);
                    else FooterAction(hintFont,"B","BACK",238,310);
                }
            }
        }
        else if( stats || gameStats )
        {
            Scrooby::Page* page=screen->GetScroobyScreen()->GetPage(gameStats?"ScrapBookStats":"LevelStats");
            Scrooby::Group* group=page ? page->GetGroup("Stats") : NULL;
            Scrooby::Text* heading=page ? page->GetText(gameStats?"GameStats":"LevelProgress") : NULL;
            const char* names[9][2]={
                {"StoryMissions","StoryMissions_Unlocked"},
                {"BonusMissions","BonusMissions_Unlocked"},
                {"StreetRaces","StreetRaces_Unlocked"},
                {"Cards","Cards_Unlocked"},
                {"Clothing","Clothing_Unlocked"},
                {"Vehicles","Vehicles_Unlocked"},
                {"Wasps","Wasps_Unlocked"},
                {"Gags","Gags_Unlocked"},
                {"LevelComplete","LevelComplete_Value"}
            };
            Scrooby::Text* firstLabel=group ? group->GetText(names[0][0]) : NULL;
            tFont* font=firstLabel ? firstLabel->GetFont() : NULL;
            if( font && font->GetFontHeight()>0 )
            {
                if( heading )
                {
                    const P3D_UNICODE* title=heading->GetStringBuffer();
                    float width=font->GetTextWidth(title)*16/font->GetFontHeight();
                    if( width>260 ) width=260;
                    BottomLabel(font,title,(320-width)*0.5f,23,16,Gold,260);
                }
                for( unsigned row=0;row<(gameStats?10u:9u);++row )
                {
                    const char* labelName=row<8 || !gameStats?names[row][0]:row==8?"Movies":"GameComplete";
                    const char* valueName=row<8 || !gameStats?names[row][1]:row==8?"Movies_Unlocked":"GameComplete_Value";
                    Scrooby::Text* label=group->GetText(labelName);
                    Scrooby::Text* value=group->GetText(valueName);
                    if( !label || !value ) continue;
                    const float y=gameStats?42+row*17:46+row*18,height=gameStats?14:15;
                    const tColour colour=row==(gameStats?9u:8u)?Gold:White;
                    BottomLabel(font,label->GetStringBuffer(),20,y,height,colour,194);
                    const P3D_UNICODE* number=value->GetStringBuffer();
                    float width=font->GetTextWidth(number)*height/font->GetFontHeight();
                    if( width>64 ) width=64;
                    BottomLabel(font,number,300-width,y,height,colour,64);
                }
                if(!gameStats)FooterAction(font,"A","CONTINUE",12,164);
                FooterAction(font,"B","BACK",238,310);
            }
        }
        else if( id==CGuiWindow::GUI_SCREEN_ID_LOADING )
        {
            Scrooby::Page* page=screen->GetScroobyScreen()->GetPage("Loading");
            Scrooby::Text* label=page ? page->GetText("Loading") : NULL;
            if( label && label->GetFont() )
            {
                tFont* font=label->GetFont();
                const P3D_UNICODE* text=label->GetStringBuffer();
                const float width=font->GetTextWidth(text)*16/font->GetFontHeight();
                BottomLabel(font,text,(320-width)*0.5f,175,16,White,280);
            }
            // Retain the original dynamite wick, flame and progress animation.
            if( page ) DisplayBottomDrawable(page->GetGroup("LoadingBar"));
        }
        else if( tutorial )
        {
            Scrooby::Screen* resource=screen->GetScroobyScreen();
            Scrooby::Page* page=resource ? resource->GetPage("Tutorial") : NULL;
            Scrooby::Text* message=page ? page->GetText("TutorialMessage3DS") : NULL;
            tFont* font=message ? message->GetFont() : NULL;
            if( resource && font && font->GetFontHeight()>0 )
            {
                // Keep the original gold frame and pulsing Bart artwork. Text
                // has its own measured content area and a separate footer row.
                Scrooby::Group* disable=page->GetGroup("DisableTutorial");
                Scrooby::Page* accept=resource->GetPage("Continue");
                const bool messageShown=message->IsVisible();
                const bool disableShown=disable && disable->IsVisible();
                const bool acceptShown=accept && accept->IsVisible();
                message->SetVisible(false);
                if( disable ) disable->SetVisible(false);
                if( accept ) accept->SetVisible(false);
                for( int i=0;i<resource->GetNumberOfPages();++i )
                    DisplayBottomDrawable(resource->GetPageByIndex(i));
                message->SetVisible(messageShown);
                if( disable ) disable->SetVisible(disableShown);
                if( accept ) accept->SetVisible(acceptShown);

                // Cache layout until the message or font changes. Avoid doing
                // repeated font measurements on the original ARM11 each frame.
                static P3D_UNICODE cachedText[256]={0},lines[16][256];
                static tFont* cachedFont=NULL;
                static unsigned lineCount=0;
                static float height=16;
                const P3D_UNICODE* text=message->GetStringBuffer();
                unsigned length=0;
                while( text && text[length] && length<255 ) ++length;
                if( text && (cachedFont!=font || memcmp(cachedText,text,(length+1)*sizeof(*text))!=0) )
                {
                    memcpy(cachedText,text,length*sizeof(*text));cachedText[length]=0;
                    cachedFont=font;
                    for( int candidate=16;candidate>=10;--candidate )
                    {
                        height=float(candidate);
                        lineCount=HandheldText::Wrap(cachedText,lines,[&](const P3D_UNICODE* line){
                            return font->GetTextWidth(line)*height/font->GetFontHeight()<=216;
                        });
                        if( lineCount<=16 && height+(lineCount ? lineCount-1 : 0)*(height+5)<=113 ) break;
                    }
                }
                if( lineCount && lineCount<=16 )
                {
                    const float blockHeight=height+(lineCount-1)*(height+5);
                    const float firstLine=74+(113-blockHeight)*0.5f+
                        height*font->GetFontBaseLine()/font->GetFontHeight();
                    for( unsigned row=0;row<lineCount;++row )
                        BottomLabel(font,lines[row],52,firstLine+row*(height+5),height,tColour(255,255,255),216);
                }
                P3D_UNICODE hide[32],next[32];
                p3d::AsciiToUnicode("X HIDE TUTORIALS",hide,32);
                p3d::AsciiToUnicode("A CONTINUE",next,32);
                if( disableShown ) BottomLabel(font,hide,30,220,13,tColour(225,219,234),160);
                const float nextWidth=font->GetTextWidth(next)*13/font->GetFontHeight();
                BottomLabel(font,next,290-nextWidth,220,13,tColour(225,219,234),100);
            }
            else if( resource ) for( int i=0;i<resource->GetNumberOfPages();++i )
                DisplayBottomDrawable(resource->GetPageByIndex(i));
        }
        if(tv)TVFinish();
        FlushUI();nativePass.End();

        picaBottomScreenEnd();
    }
}

void CHudMap::LoadBottomAssets3DS()
{
    // These small original images are absent from the active MultiHud export.
    // Load them once during HUD entry, before drawing; no new artwork or atlas.
    tImageHandler* handler=dynamic_cast<tImageHandler*>(p3d::loadManager->GetHandler("png"));
    if(!handler)return;
    const tImageHandler::LoadType type=handler->GetLoadType();const bool fullName=handler->GetFullName();
    handler->SetLoadType(tImageHandler::SPRITE);handler->SetFullName(false);
    const char* names[]={"camicon","coins"};
    const char* paths[]={"art/frontend/scrooby2/resource/images/hud/mapicons/camicon.png",
        "art/frontend/scrooby2/resource/images/coins.png"};
    for(unsigned i=0;i<2;++i){
        if(!p3d::find<tSprite>(names[i])){
            const radMemoryAllocator old=radMemorySetCurrentAllocator(RADMEMORY_ALLOC_TEMP);
            tLoadRequest* request=new tLoadRequest(paths[i]);radMemorySetCurrentAllocator(old);
            request->SetMemoryAllocator(GMA_LEVEL_HUD);request->SetAsync(false);
            p3d::loadManager->Load(request);
        }
        tSprite* icon=UIAsset(names[i]);
        if(icon && icon->GetShader())icon->GetShader()->SetInt(PDDI_SP_FILTER,PDDI_FILTER_NONE);
    }
    handler->SetLoadType(type);handler->SetFullName(fullName);
}

void CHudMap::HandleScreenInput3DS()
{
    CGuiScreen* screen=CurrentBottomScreen();
    if(!screen || screen->HasMenu() || !screen->IsRunning())return;
    const auto id=screen->GetWindowID();
    const bool gameplay=id==CGuiWindow::GUI_SCREEN_ID_HUD && GetGameFlow()->GetCurrentContext()==CONTEXT_GAMEPLAY;
    const bool card=id==CGuiWindow::GUI_SCREEN_ID_CARD_GALLERY || id==CGuiWindow::GUI_SCREEN_ID_VIEW_CARDS;
    const bool conversation=ConversationButtons(screen)!=NULL;
    const bool messageOnly=id==CGuiWindow::GUI_SCREEN_ID_GENERIC_MESSAGE;
    const bool credits=id==CGuiWindow::GUI_SCREEN_ID_VIEW_CREDITS;
    if(!gameplay && !Briefing(screen) && id!=CGuiWindow::GUI_SCREEN_ID_TUTORIAL && id!=CGuiWindow::GUI_SCREEN_ID_LEVEL_STATS && id!=CGuiWindow::GUI_SCREEN_ID_SCRAP_BOOK_STATS && !card && !conversation && !messageOnly && !credits)return;
    static CGuiScreen* owner=NULL;static int pressed=-1,last=-1,startY=0,lastY=0;
    static picaBottomFrame::TouchEdges edges;static unsigned epoch=~0u;
    if(owner!=screen || epoch!=s_touchOwner.epoch){owner=screen;epoch=s_touchOwner.epoch;pressed=last=-1;
        s_hudActionPressed=false;edges.Reset((hidKeysHeld()&KEY_TOUCH)!=0);}
    edges.Update((hidKeysHeld()&KEY_TOUCH)!=0);
    touchPosition touch;hidTouchRead(&touch);
    int hit=touch.py>=212 ? (touch.px<220?0:1) : -1;
    if(card && touch.px<220)hit=-1;
    if(credits)hit=picaBottomFrame::Rect{44,146,276,178}.Contains(touch.px,touch.py)?0:-1;
    if(conversation)hit=picaBottomFrame::Rect{16,86,304,132}.Contains(touch.px,touch.py)?0:
        picaBottomFrame::Rect{16,146,304,186}.Contains(touch.px,touch.py)?1:-1;
    if(gameplay){
        Scrooby::Page* page=screen->GetScroobyScreen()->GetPage("Hud");
        Scrooby::Group* action=page?page->GetGroup("ActionButton"):NULL;
        hit=action && action->IsVisible() && picaBottomFrame::Interaction.Contains(touch.px,touch.py)?2:-1;
    }
    if(edges.down){pressed=hit;startY=lastY=touch.py;}
    if(edges.held){last=hit;lastY=touch.py;if(gameplay)s_hudActionPressed=pressed==2 && hit==2;}
    if(edges.up){
        s_hudActionPressed=false;
        const int commit=pressed;pressed=-1;
        if(commit<0 && abs(lastY-startY)>20 && (!gameplay || (startY>=184 && lastY>=184))){s_textScrollDelta=lastY<startY?2:-2;return;}
        if(commit<0 || commit!=last)return;
        if(gameplay){
#ifdef SRR2_OLD_3DS
            old3dsTouchActionRequest();
#endif
            return;
        }
        if(credits){screen->HandleMessage(GUI_MSG_CONTROLLER_SELECT,GetGuiSystem()->GetPrimaryController());return;}
        if(id==CGuiWindow::GUI_SCREEN_ID_TUTORIAL){
            if(commit==0){
                Scrooby::Page* page=screen->GetScroobyScreen()->GetPage("Tutorial");
                Scrooby::Group* hide=page?page->GetGroup("DisableTutorial"):NULL;
                if(hide && hide->IsVisible())screen->HandleMessage(GUI_MSG_CONTROLLER_AUX_X,GetGuiSystem()->GetPrimaryController());
            }else screen->HandleMessage(GUI_MSG_CONTROLLER_SELECT,GetGuiSystem()->GetPrimaryController());
            return;
        }
        if(conversation && commit==0){screen->HandleMessage(GUI_MSG_CONTROLLER_SELECT,GetGuiSystem()->GetPrimaryController());return;}
        if((id==CGuiWindow::GUI_SCREEN_ID_SCRAP_BOOK_STATS || card) && commit==1){
            screen->HandleMessage(GUI_MSG_CONTROLLER_BACK,GetGuiSystem()->GetPrimaryController());return;
        }
        if(id==CGuiWindow::GUI_SCREEN_ID_LEVEL_STATS){
            screen->HandleMessage(commit==0?GUI_MSG_CONTROLLER_SELECT:GUI_MSG_CONTROLLER_BACK,GetGuiSystem()->GetPrimaryController());return;
        }
        const eButtonIcon button=commit==0?BUTTON_ICON_ACCEPT:BUTTON_ICON_BACK;
        if(screen->IsButtonVisible(button))screen->HandleMessage(commit==0?GUI_MSG_CONTROLLER_SELECT:GUI_MSG_CONTROLLER_BACK,GetGuiSystem()->GetPrimaryController());
    }
}

void CHudMap::NotifyCoins3DS(int amount)
{
    const u64 now=svcGetSystemTick();
    if(!s_pickupTick || float(now-s_pickupTick)/SYSCLOCK_ARM11>1.2f)s_pickupDelta=0;
    s_pickupDelta+=amount;s_pickupTick=now;
    if(amount>0)s_pickups[s_nextPickup++%8]={now,amount};
}

bool CHudMap::HandleMenuInput3DS(CGuiMenu* menu,int rawMessage)
{
    CGuiScreen* screen=CurrentBottomScreen();
    if(!screen || screen->HasMenu()!=menu || !screen->IsRunning() || menu->HasSelectionBeenMade())return false;
    const bool main=screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_MAIN_MENU;
    const bool rewards=Rewards(screen),tv=Television(screen);

    int rows[64];int count=MenuRows(menu,rows);
    const bool sprites=count==0;
    if(sprites)count=MenuSprites(menu,rows);
    if(!count)return false;
    if(main)count=menu->GetMenuItem(0)->m_itemValueCount;
    if(count<1 || count>64)return false;
    int selected=0;if(!main)for(int i=0;i<count;++i)if(rows[i]==menu->GetSelection())selected=i;
    if(main)selected=picaBottomFrame::MainRow(menu->GetSelectionValue(0),count);
    const eGuiMessage message=static_cast<eGuiMessage>(rawMessage);
    if(main && menu->GetSelection()==0 && message>=GUI_MSG_CONTROLLER_UP && message<=GUI_MSG_CONTROLLER_RIGHT){
        const int direction=message==GUI_MSG_CONTROLLER_UP || message==GUI_MSG_CONTROLLER_LEFT?-1:1;
        const int next=(selected+direction+count)%count;
        menu->SetSelectionValue(0,picaBottomFrame::MainChoice(next,count));
        GetEventManager()->TriggerEvent(EVENT_FE_MENU_UPORDOWN);
        MainScrollPosition(menu);return true;
    }
    if(PauseGrid(screen) && message>=GUI_MSG_CONTROLLER_UP && message<=GUI_MSG_CONTROLLER_RIGHT){
        const int dx=message==GUI_MSG_CONTROLLER_LEFT?-1:message==GUI_MSG_CONTROLLER_RIGHT?1:0;
        const int dy=message==GUI_MSG_CONTROLLER_UP?-1:message==GUI_MSG_CONTROLLER_DOWN?1:0;
        int next=picaBottomFrame::PauseMove(selected,count,dx,dy);
        if(!menu->IsMenuItemEnabled(rows[next])){
            const int start=next;do {next=(next+1)%count;}while(next!=start && !menu->IsMenuItemEnabled(rows[next]));
        }
        if(menu->IsMenuItemEnabled(rows[next]))menu->SetHandheldSelection(rows[next]);
        return true;
    }
    if(message!=GUI_MSG_UPDATE)return false;
    static CGuiMenu* gestureMenu=NULL;static int pressed=-1;
    static bool active=false,sliderDrag=false;static int sliderItem=-1,lastHit=-1,startY=0,lastY=0,lastX=0;
    static picaBottomFrame::TouchEdges edges;static unsigned epoch=~0u;
    if(gestureMenu!=menu || epoch!=s_touchOwner.epoch){gestureMenu=menu;epoch=s_touchOwner.epoch;pressed=-1;active=false;sliderDrag=false;sliderItem=-1;s_pressedMenu=NULL;s_pressedItem=-1;s_mainDragging=false;edges.Reset((hidKeysHeld()&KEY_TOUCH)!=0);}
    edges.Update((hidKeysHeld()&KEY_TOUCH)!=0);
    touchPosition touch;hidTouchRead(&touch);
    const int pageSize=sprites?8:tv?picaBottomFrame::TVPageSize:6;
    const int first=main?0:(selected/pageSize)*pageSize,visible=count-first<pageSize?count-first:pageSize;
    const float mainScroll=main?MainScrollPosition(menu):0;
    static float mainDragStart=0;static bool mainDragAllowed=false;
    int hit=-1;
    if(PauseGrid(screen))hit=picaBottomFrame::PauseHit(touch.px,touch.py,count);
    else for(int row=0;row<visible;++row)
        if(NativeMenuItem(screen,row,visible).Contains(touch.px,touch.py)){hit=row;break;}
    if(main && tv)hit=picaBottomFrame::MainHit(touch.px,touch.py,count,mainScroll);
    if(sprites)hit=tv?picaBottomFrame::TVThumbnailHit(touch.px,touch.py,visible):picaBottomFrame::ThumbnailHit(touch.px,touch.py,visible);
    if(tv && picaBottomFrame::Rect{44,190,276,208}.Contains(touch.px,touch.py))hit=touch.px>=220?
        (screen->IsButtonVisible(BUTTON_ICON_BACK)?201:-1):
        !main && ChangesValue(menu)?(touch.px<80?202:touch.px<120?203:-1):200;
    if(!tv && touch.py>=212 && !rewards){
        if(PauseGrid(screen))hit=touch.px>=164?201:touch.px<140?200:-1;
        else hit=touch.px>=220?(screen->IsButtonVisible(BUTTON_ICON_BACK)?201:-1):
            !main && ChangesValue(menu)?(touch.px<50?202:touch.px<90?203:-1):200;
    }
    if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_MISSION_SELECT && touch.py<32)
        hit=touch.px>=218 && touch.px<238?204:touch.px>=298?205:-1;
    if(screen->GetWindowID()==CGuiWindow::GUI_SCREEN_ID_SCRAP_BOOK_CONTENTS && touch.py>=(tv?35:4) && touch.py<(tv?57:30))
        hit=touch.px>=(tv?216:248) && touch.px<(tv?240:272)?206:touch.px>=(tv?248:288) && touch.px<(tv?272:316)?207:-1;
    if(rewards)hit=touch.py>=40 && touch.py<84 ? (touch.px<44?100:touch.px>=276?101:-1) :
        touch.py>=212 ? (touch.px<212?102:103):-1;
    if(edges.held){lastHit=hit;lastY=touch.py;lastX=touch.px;
        if(active && mainDragAllowed && abs(lastY-startY)>6){
            s_mainDragging=true;
            s_mainScroll.Drag(mainDragStart+(startY-lastY)/26.0f,count,static_cast<unsigned>(osGetTime()));
            lastHit=-1;s_pressedMenu=NULL;s_pressedItem=-1;
        }
        if(active && !s_mainDragging){s_pressedMenu=menu;s_pressedItem=hit==pressed?(main?menu->GetSelectionValue(0):menu->GetSelection()):-1;}

        if(active && sliderDrag && pressed>=0 && hit==pressed && !main && !sprites && !rewards &&
           hit>=0 && hit<(PauseGrid(screen)?count:visible)){
            const int index=PauseGrid(screen)?hit:first+hit;
            GuiMenuItem* item=menu->GetMenuItem(rows[index]);
            if(item->m_slider.m_pPolygon || item->m_slider.m_pImage){
                float value=picaBottomFrame::SliderValue(touch.px,tv);
                if(value!=item->m_slider.m_value){item->m_slider.SetValue(value);screen->HandleMessage(GUI_MSG_MENU_SELECTION_VALUE_CHANGED,rows[index]);}
            }
        }
    }
    if(edges.down){
        active=true;sliderDrag=false;sliderItem=-1;pressed=hit;startY=touch.py;lastY=touch.py;lastX=touch.px;
        if(main){s_mainDragging=false;mainDragStart=mainScroll;}
        mainDragAllowed=main && tv && picaBottomFrame::Rect{44,58,276,188}.Contains(touch.px,touch.py);
        s_pressedMenu=menu;s_pressedItem=menu->GetSelection();
        if(hit>=0){const int index=PauseGrid(screen)?hit:first+hit;
            if(rewards || hit>=200){s_pressedItem=-1;}
            else if(main){const int value=picaBottomFrame::MainChoice(hit,count);menu->SetHandheldSelection(0);menu->SetSelectionValue(0,value);s_pressedItem=value;}
            else if(menu->IsMenuItemEnabled(rows[index])){
                menu->SetHandheldSelection(rows[index]);
                GuiMenuItem* item=menu->GetMenuItem(rows[index]);
                sliderItem=(item->m_slider.m_pPolygon || item->m_slider.m_pImage)?rows[index]:-1;
                sliderDrag=sliderItem>=0 && picaBottomFrame::SliderTrackHit(touch.px,tv);
            }else pressed=-1;}
    }
    if(active && edges.up){
        const int previous=pressed;active=false;pressed=-1;s_pressedMenu=NULL;s_pressedItem=-1;
        if(main && s_mainDragging){
            s_mainDragging=false;const unsigned now=static_cast<unsigned>(osGetTime());
            const int firstRow=int(s_mainScroll.Value(now)+0.5f);
            s_mainScroll.Move(float(firstRow),count,now);
            if(selected<firstRow)menu->SetSelectionValue(0,picaBottomFrame::MainChoice(firstRow,count));
            else if(selected>=firstRow+5)menu->SetSelectionValue(0,picaBottomFrame::MainChoice(firstRow+4,count));
            return false;
        }
        if(sliderItem>=0){
            screen->HandleMessage(GUI_MSG_MENU_SLIDER_NOT_CHANGING,sliderItem);
            sliderItem=-1;sliderDrag=false;return false;
        }
        if(Question(screen) && previous<0 && abs(lastY-startY)>20){s_textScrollDelta=lastY<startY?2:-2;return false;}
        if(!main && !rewards && !PauseGrid(screen) && count>pageSize && abs(lastY-startY)>32){
            int next=lastY<startY?first+pageSize:first-1;
            if(next<0)next=0;if(next>=count)next=count-1;
            menu->SetHandheldSelection(rows[next]);return false;
        }
        // Release over the same item commits. Dragging out cancels; no double tap.
        if(previous>=0 && previous==lastHit){
            if(previous>=200){
                if(previous==201)screen->HandleMessage(PauseGrid(screen)?GUI_MSG_CONTROLLER_START:GUI_MSG_CONTROLLER_BACK,GetGuiSystem()->GetPrimaryController());
                else if(previous==206 || previous==207)screen->HandleMessage(previous==206?GUI_MSG_CONTROLLER_L1:GUI_MSG_CONTROLLER_R1,GetGuiSystem()->GetPrimaryController());
                else if(previous>=202 && previous<=205)screen->HandleMessage((previous==202 || previous==204)?GUI_MSG_CONTROLLER_LEFT:GUI_MSG_CONTROLLER_RIGHT,GetGuiSystem()->GetPrimaryController());
                else {
                    GuiMenuItem* item=menu->GetSelection()>=0?menu->GetMenuItem(menu->GetSelection()):NULL;
                    if(item && !(item->m_attributes&SELECTABLE) && item->m_itemValueCount>1)
                        screen->HandleMessage(GUI_MSG_CONTROLLER_RIGHT,GetGuiSystem()->GetPrimaryController());
                    else screen->HandleMessage(GUI_MSG_CONTROLLER_SELECT,GetGuiSystem()->GetPrimaryController());
                }
            }else if(rewards){
                const eGuiMessage action=previous==100?GUI_MSG_CONTROLLER_LEFT:previous==101?GUI_MSG_CONTROLLER_RIGHT:
                    previous==103?GUI_MSG_CONTROLLER_BACK:GUI_MSG_CONTROLLER_SELECT;
                if(action!=GUI_MSG_CONTROLLER_SELECT || screen->IsButtonVisible(BUTTON_ICON_ACCEPT))
                    screen->HandleMessage(action,GetGuiSystem()->GetPrimaryController());
            }else {
                GuiMenuItem* item=menu->GetMenuItem(main?0:rows[PauseGrid(screen)?previous:first+previous]);
                if(item->m_slider.m_pPolygon || item->m_slider.m_pImage)screen->HandleMessage(GUI_MSG_MENU_SLIDER_NOT_CHANGING,menu->GetSelection());
                else if(!main && item->m_itemValueCount>1 && (lastX>=(tv?150:202) || !(item->m_attributes&SELECTABLE)))
                    screen->HandleMessage(lastX>=(tv?150:202) && lastX<(tv?208:250)?GUI_MSG_CONTROLLER_LEFT:GUI_MSG_CONTROLLER_RIGHT,GetGuiSystem()->GetPrimaryController());
                else screen->HandleMessage(GUI_MSG_CONTROLLER_SELECT,GetGuiSystem()->GetPrimaryController());
            }
        }
    }
    return false;
}

void CHudMap::AfterRenderLayers3DS( tView* guiView )
{
    for( unsigned i=0;i<s_bottomDialogCount;++i )
        s_bottomDialogPages[i]->SetVisible(s_bottomDialogVisible[i]);
    for( unsigned i=0;i<s_bottomActionCount;++i ){
        s_bottomActions[i]->SetVisible(s_bottomActionVisible[i]);
        s_bottomActions[i]->SetAlpha(s_bottomActionAlpha[i]);
    }
    CHudMap* map = s_bottomMap;
    if( map != NULL )
    {
        map->m_hudMapGroup->SetVisible( s_mapWasVisible );
        if( s_bottomBackground != NULL )
        {
            s_bottomBackground->SetVisible( s_backgroundWasVisible );
        }
    }

    DrawUpperPreview(guiView,CurrentBottomScreen());
    DrawUpperStats(guiView,CurrentBottomScreen());
    DrawUpperObjective(guiView,CurrentBottomScreen());
    DrawUpperCredits(guiView,CurrentBottomScreen());
    const ContextEnum context=GetGameFlow()->GetCurrentContext();
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
    CGuiManagerInGame* cadenceManager=GetGuiSystem()->GetInGameManager();
    const bool cadenceEligible=context==CONTEXT_GAMEPLAY && map && guiView && s_mapWasVisible &&
        (!map->m_hudLayer || map->m_hudLayer->IsVisible()) && cadenceManager &&
        cadenceManager->GetCurrentScreen()==CGuiWindow::GUI_SCREEN_ID_HUD;
    if(!cadenceEligible)Old3dsBottomMap::Reset();
#endif
    if( context != CONTEXT_GAMEPLAY && context != CONTEXT_PAUSE )
    {
        DisplayBottomInterface(guiView);
        return;
    }
    CGuiManagerInGame* inGame = GetGuiSystem()->GetInGameManager();
    if(guiView==NULL || inGame==NULL || inGame->GetCurrentScreen()!=CGuiWindow::GUI_SCREEN_ID_HUD){
        DisplayBottomInterface(guiView);return;
    }
    const bool shown=map && s_mapWasVisible && (!map->m_hudLayer || map->m_hudLayer->IsVisible());
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
    if(shown && !Old3dsBottomMap::Due())return;
#endif
    DrawHUD(guiView);
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
    if(shown)Old3dsBottomMap::Rendered();
#endif

}

void CHudMap::GetBottomScreenSource( float rect[ 4 ] ) const
{
    int x0 = 0, y0 = 0, x1 = 640, y1 = 480;
    Scrooby::Sprite* ring = ( m_radar != NULL ) ? m_radar->GetSprite( "Radar0" ) : NULL;
    if( ring != NULL )
    {
        ring->GetBoundingBox( x0, y0, x1, y1 );
    }

    const float sw = (float)Scrooby::App::GetInstance()->GetScreenWidth();
    const float sh = (float)Scrooby::App::GetInstance()->GetScreenHeight();
    const float cx = 0.5f * ( x0 + x1 );
    const float cy = 0.5f * ( y0 + y1 );
    const float half = 0.38f * ( y1 - y0 );

    rect[ 0 ] = ( cx - half ) / sw;
    rect[ 2 ] = ( cx + half ) / sw;
    rect[ 1 ] = 1.0f - ( cy + half ) / sh;
    rect[ 3 ] = 1.0f - ( cy - half ) / sh;
}

static void DisplayChild( Scrooby::Group* group )
{
    float x = 0.0f, y = 0.0f;
    p3d::stack->Push();
    p3d::stack->Multiply( *group->GetTransform() );
    group->GetNormalizedPosition( x, y );
    p3d::stack->Translate( x, y, 0.0f );
    group->Display();
    p3d::stack->Pop();
}

void CHudMap::DisplayOnBottomScreen( tView* guiView, Scrooby::Group* background )
{
    // BeginRender changes both the camera and the current matrix. Preserve the
    // caller before it runs, so subsequent native HUD text is not near-clipped.
    tView* previousView=p3d::context->GetView();
    p3d::pddi->PushState(PDDI_STATE_RENDER);
    p3d::pddi->PushState(PDDI_STATE_VIEW);
    p3d::stack->Push();
    const unsigned clear=guiView->GetClearMask();guiView->SetClearMask(0);
    guiView->BeginRender();guiView->SetClearMask(clear);

    const bool wide = p3d::display->IsWidescreen();
    if( wide )
    {
        m_p3dMap->SetWideScreenCorrectionEnabled( false );
        if( m_p3dHole != NULL )
        {
            m_p3dHole->SetWideScreenCorrectionEnabled( false );
        }
    }

    p3d::pddi->EnableZBuffer( false );

    const float aspect = (float)Scrooby::App::GetInstance()->GetScreenWidth() /
                         (float)Scrooby::App::GetInstance()->GetScreenHeight();
    p3d::pddi->SetCamera( 0.1f, 10.0f, 1.5708f, aspect );
    p3d::pddi->SetProjectionMode( PDDI_PROJECTION_PERSPECTIVE );
    p3d::pddi->SetCullMode( PDDI_CULL_NONE );
    p3d::stack->Push();
    p3d::stack->LoadIdentity();
    p3d::stack->Translate( -0.5f, -0.5f / aspect, 0.5f );

    if( background != NULL )
    {
        DisplayChild( background );
    }
    DisplayChild( m_hudMapGroup );

    p3d::stack->Pop();
    guiView->EndRender();
    p3d::context->SetView(previousView);
    p3d::stack->Pop();
    p3d::pddi->PopState(PDDI_STATE_VIEW);
    p3d::pddi->PopState(PDDI_STATE_RENDER);

    if( wide )
    {
        m_p3dMap->SetWideScreenCorrectionEnabled( true );
        if( m_p3dHole != NULL )
        {
            m_p3dHole->SetWideScreenCorrectionEnabled( true );
        }
    }
}
#endif

void
CHudMap::RestoreAllRegisteredIcons()
{
    if( s_numRegisteredIcons > 0 )
    {
        int numIconsRestored = 0;

        for( unsigned int i = 0; i < MAX_NUM_REGISTERED_ICONS; i++ )
        {
            if( s_registeredIcons[ i ].m_iconImage != NULL )
            {
                s_registeredIcons[ i ].m_iconImage = this->RemoveIconFromInventory( s_registeredIcons[ i ].m_type );
                rAssert( s_registeredIcons[ i ].m_iconImage != NULL );

                if( s_registeredIcons[ i ].IsAICarIcon() )
                {
                    s_registeredIcons[ i ].ApplyAICarIconColour();
                }

                numIconsRestored++;
            }
        }

        rAssertMsg( numIconsRestored == s_numRegisteredIcons, "Not all registered icons were restored!" );
    }
}

void
CHudMap::ClearAllRegisteredIcons()
{
    memset( s_registeredIcons, 0, sizeof( s_registeredIcons ) );
    s_numRegisteredIcons = 0;
    s_fpIconID = -1;
}

//===========================================================================
// Private Member Functions
//===========================================================================

//===========================================================================
// CHudMap::CalculateDistanceBetweenPoints
//===========================================================================
// Description: Calculates the scalar distance between 2 points in 3D-space.
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
float
CHudMap::CalculateDistanceBetweenPoints( rmt::Vector& a, rmt::Vector& b )
{
    rmt::Vector displacement( 0.0f, 0.0f, 0.0f );
    displacement.Sub( a, b );

    return displacement.Magnitude();
}

//===========================================================================
// CHudMap::UpdateIconHeading
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
void
CHudMap::UpdateIconHeading( Scrooby::Sprite* iconImage, rmt::Vector* iconHeading )
{
    rmt::Vector objectHeading( iconHeading->x, 0, iconHeading->z );

    rmt::Vector camHeading;
    tPointCamera* camera = (tPointCamera*)m_p3dMap->GetCamera();
    rAssert( camera );
    camera->GetVUp( &camHeading );
    camHeading.y = 0.0f;

    if( camHeading.MagnitudeSqr() > 0.0f && objectHeading.MagnitudeSqr() > 0.0f )
    {
        float ratio = camHeading.Dot( objectHeading ) /
                      (camHeading.Magnitude() * objectHeading.Magnitude());

        float rotation = 0.0f;

        if( ratio > 1.0f )
        {
            rotation = 0.0f;
        }
        else if( ratio < -1.0f )
        {
            rotation = rmt::PI;
        }
        else
        {
            rotation = rmt::ACos( ratio );
        }

        rAssert( !rmt::IsNan( rotation ) );

        rmt::Vector normal = camHeading;
        normal.CrossProduct( objectHeading );
        if( normal.y < 0 )
        {
            rotation = -rotation;
        }

        rAssert( iconImage );
        iconImage->RotateAboutCenter( rmt::RadianToDeg( rotation ) );
    }
}

//===========================================================================
// CHudMap::CalculateCameraHeight
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
float
CHudMap::CalculateCameraHeight( rmt::Vector& player, rmt::Vector& mission ) const
{
    // fix locations at sea level
    //
    player.y = 0;
    mission.y = 0;

    float distance = CalculateDistanceBetweenPoints( player, mission );

    // calculate camera height based on distance
    //
    float cameraHeight = this->CalculateCameraHeight( distance * 1.5f );

    if( cameraHeight < MIN_CAMERA_HEIGHT )
    {
        cameraHeight = MIN_CAMERA_HEIGHT;
    }
    else if( cameraHeight > MAX_CAMERA_HEIGHT )
    {
        cameraHeight = MAX_CAMERA_HEIGHT;
    }

    return cameraHeight;
}

//===========================================================================
// CHudMap::CalculateCameraHeight
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
float
CHudMap::CalculateCameraHeight( float visibleRadius ) const
{
    float cameraHeight = visibleRadius;
    if( rmt::Abs( m_hudMapCam->GetFOV() - rmt::PI_BY2 ) > 0.001f )
    {
        cameraHeight = visibleRadius / rmt::Tan( m_hudMapCam->GetFOV() / 2.0f );
    }

    return cameraHeight;
}

//===========================================================================
// CHudMap::CalculatRadarConeAngle
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================
float
CHudMap::CalculatRadarConeAngle( rmt::Vector& iconLoc ) const
{
    float radarConeAngle = 0.0f; // zero angle by default

    if( iconLoc.y > 0.0f ) // icon is located in either Quadrant I or II
    {
        radarConeAngle = rmt::ATan( iconLoc.x / iconLoc.y );
    }
    else if( iconLoc.y < 0.0f ) // icon is located in either Quadrant III or IV
    {
        radarConeAngle = rmt::ATan( iconLoc.x / iconLoc.y ) + rmt::PI;
    }
    else // icon is located somewhere along the X-axis
    {
        if( iconLoc.x > 0.0f )
        {
            radarConeAngle = rmt::PI_BY2;
        }
        else if( iconLoc.x < 0.0f )
        {
            radarConeAngle = -rmt::PI_BY2;
        }
    }

    return radarConeAngle;
}

//===========================================================================
// CHudMap::DetermineOnRoadLocation
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      
//
//===========================================================================

// We return the target vector (where we want to direct the navigation cone)
void
CHudMap::DetermineOnRoadLocation( rmt::Vector& source, rmt::Vector& target, float visibleRadius )
{
    // place source and target at sea level
    //
    source.y = 0;
    target.y = 0;

    float distance = CalculateDistanceBetweenPoints( source, target );
    if( distance <= visibleRadius )
    {
        // no need to do any calculations if target is within 'visibleRadius' from source
        //
        return;
    }

    // check to see if we should skip this update and return cached information instead
    //
    m_frameCount = (m_frameCount + 1) % NUM_AMORTIZED_UPDATE_FRAMES;
    if( m_frameCount != 0 )
    {
        target = m_lastOnRoadLocation;

        return;
    }

    // get reference to road manager
    //
    RoadManager* roadManager = RoadManager::GetInstance();
    rAssert( roadManager != NULL );

    //////////////
    // find path between source and target
    //
    bool isPathElementFound = false;
    float searchRadius = 200.0f;
    float dummy;

    // Find source's closest path element
    RoadSegment* sourceSeg = NULL;
    float sourceSegT = 0.0f;
    RoadManager::PathElement sourceElem;
    float sourceRoadT = 0.0f;

    isPathElementFound = VehicleAI::FindClosestPathElement( source, 
        sourceElem, sourceSeg, sourceSegT, sourceRoadT, false ); // don't want shortcuts on hudmap

    if( !isPathElementFound )
    {
        GetIntersectManager()->FindClosestRoad( source, searchRadius, sourceSeg, dummy ); // ignore shortcuts
        rAssertMsg( sourceSeg, "Should always find a nearby road seg! Increase search radius!" );
        if( !sourceSeg )
        {
            return;
        }

        sourceElem.type = RoadManager::ET_NORMALROAD;
        sourceElem.elem = sourceSeg->GetRoad();
        sourceSegT = roadManager->DetermineSegmentT( source, sourceSeg );
        sourceRoadT = roadManager->DetermineRoadT( sourceSeg, sourceSegT );
    }
    rAssert( sourceElem.elem );


    // Find target's closest path element
    RoadSegment* targetSeg = NULL;
    float targetSegT = 0.0f;
    RoadManager::PathElement targetElem;
    float targetRoadT = 0.0f;

    isPathElementFound = VehicleAI::FindClosestPathElement( target, 
        targetElem, targetSeg, targetSegT, targetRoadT, false ); // don't want shortcuts on hudmap

    if( !isPathElementFound )
    {
        RoadSegment* closestRoadSeg = NULL;
        if( m_lastRoadSeg )
        {
            closestRoadSeg = m_lastRoadSeg;
        }
        else
        {
            GetIntersectManager()->FindClosestRoad( target, searchRadius, closestRoadSeg, dummy );
            rAssertMsg( closestRoadSeg, "Should always find a nearby road seg! Increase search radius!" );
            if( !closestRoadSeg )
            {
                return;
            }
            m_lastRoadSeg = closestRoadSeg;
        }
        targetElem.type = RoadManager::ET_NORMALROAD;
        targetElem.elem = closestRoadSeg->GetRoad();
        targetSeg = closestRoadSeg;
        targetSegT = roadManager->DetermineSegmentT( target, targetSeg );
        targetRoadT = roadManager->DetermineRoadT( targetSeg, targetSegT );
    }
    else
    {
        // need to make this check cuz FindClosestPathElement could return an intersection
        if( targetSeg ) 
        {
            m_lastRoadSeg = targetSeg;
        }
    }
    rAssert( targetElem.elem );


    HeapMgr()->PushHeap( GMA_TEMP );

    SwapArray<RoadManager::PathElement> pathElements;
    pathElements.Allocate( roadManager->GetMaxPathElements() );

    RoadManager::PathElement tmpSrcElem = sourceElem;
    float roadDistance = roadManager->FindPathElementsBetween( false,
                                                               tmpSrcElem, sourceRoadT, source,
                                                               targetElem, targetRoadT, target,
                                                               pathElements );

    rAssert( pathElements.mUseSize > 0 );

    // Temp stuff we use over and over again
    RoadManager::PathElement* prevElem = &(pathElements[0]);
    bool isRoadBackwards = false;
    int numIntersects = 0;
    rmt::Vector intPts[2];

    rmt::Sphere visibleSphere( source, visibleRadius );     // this is the hud map circle

    // find closest path element that intersects visible sphere (or circle, really)
    //
    bool isClosestPathElementFound = false;

    if( pathElements.mUseSize == 1 )
    {
        // if only one element returned
        if( pathElements[0].type == RoadManager::ET_INTERSECTION )
        {
            // if the only elem is an intersection, just point to target
            numIntersects = IntersectLineSphere( source, target, visibleSphere, intPts );
            rAssert( numIntersects == 1 );
            target = intPts[0];
        }
        else // the only element is a road
        {
            Road* theRoad = (Road*) targetElem.elem;

            // we know here that one element returned is either cuz:
            // A) source and target lie on same road 
            // or 
            // B) source and target lie on opposite roads that
            // describe the same physical road (target road is returned)

            // case B
            if( sourceElem != targetElem ) 
            {
                // iterate through target road's segments to find the closest seg
                // to source, the t value of this segment will tell us whether to
                // traverse the segments backwards or forwards...
                //
                rmt::Vector closestPos;
                float closestDist;
                int closestSegIndex;

                RoadManager::FindClosestPointOnRoad( theRoad, source, closestPos, closestDist, closestSegIndex );

                rAssert( 0 <= closestSegIndex && closestSegIndex < (int) theRoad->GetNumRoadSegments() );
                RoadSegment* closestSeg = theRoad->GetRoadSegment( (unsigned int) closestSegIndex );
                rAssert( closestSeg );

                sourceElem.elem = theRoad;
                sourceSeg = closestSeg;
                sourceSegT = RoadManager::DetermineSegmentT( closestPos, closestSeg );
                sourceRoadT = RoadManager::DetermineRoadT( sourceSeg, sourceSegT );
            }

            rAssert( sourceElem == targetElem );

            // this should now work for either A or B
            if( sourceRoadT > targetRoadT )
            {
                isRoadBackwards = true;
            }

            unsigned int startIndex = sourceSeg->GetSegmentIndex();
            unsigned int endIndex = targetSeg->GetSegmentIndex();

            bool foundIntersect = false;
            if( !isRoadBackwards )
            {
                for( unsigned int i=startIndex; i<=endIndex; i++ )
                {
                    RoadSegment* seg = theRoad->GetRoadSegment( i );

                    rmt::Vector vec0, vec1, vec2, vec3, start, end;
                    seg->GetCorner( 0, vec0 );
                    seg->GetCorner( 1, vec1 );
                    seg->GetCorner( 2, vec2 );
                    seg->GetCorner( 3, vec3 );

                    start = (vec0 + vec3) * 0.5f;
                    start.y = 0.0f;
                    end = (vec1 + vec2) * 0.5f;
                    end.y = 0.0f;

                    int numIntersects = IntersectLineSphere( start, end, visibleSphere, intPts );
                    rAssert( 0 <= numIntersects && numIntersects <= 1 );

                    if( numIntersects == 1 )
                    {
                        target = intPts[0];
                        isClosestPathElementFound = true;

                        break;
                    }
                }
            }
            else
            {
                for( int i=(int)startIndex; i>=(int)endIndex; i-- )
                {
                    RoadSegment* seg = theRoad->GetRoadSegment( (unsigned int)i );

                    rmt::Vector vec0, vec1, vec2, vec3, start, end;
                    seg->GetCorner( 0, vec0 );
                    seg->GetCorner( 1, vec1 );
                    seg->GetCorner( 2, vec2 );
                    seg->GetCorner( 3, vec3 );

                    start = (vec0 + vec3) * 0.5f;
                    start.y = 0.0f;
                    end = (vec1 + vec2) * 0.5f;
                    end.y = 0.0f;

                    int numIntersects = IntersectLineSphere( start, end, visibleSphere, intPts );
                    rAssert( 0 <= numIntersects && numIntersects <= 1 );

                    if( numIntersects == 1 )
                    {
                        target = intPts[0];
                        isClosestPathElementFound = true;

                        break;
                    }
                }
            }
        }
    }
    else
    {
        // At this point, we're dealing with multiple path elements
        //
        Intersection* previousIntersection = NULL;
        for( int i = 0; i < pathElements.mUseSize; i++ )
        {
            if( isClosestPathElementFound )
            {
                // we're done! we found the closest path element
                //
                break;
            }

            RoadManager::PathElement* currentPathElement = &( pathElements[ i ] );
            rAssert( currentPathElement != NULL );

            switch( currentPathElement->type )
            {
                case RoadManager::ET_NORMALROAD:
                {
                    Road* road = static_cast<Road*>( currentPathElement->elem );
                    rAssert( road != NULL );

                    int numRoadSegments = static_cast<int>( road->GetNumRoadSegments() );

                    // determine which direction to iterate over all road segments
                    //
                    bool isRoadBackwards = false;

                    if( i == 0 ) // special case if first path element is a road
                    {
                        for( int j = 1; j < pathElements.mUseSize; j++ )
                        {
                            RoadManager::PathElement* pathElement = &( pathElements[ j ] );
                            rAssert( pathElement != NULL );

                            if( pathElement->type == RoadManager::ET_INTERSECTION )
                            {
                                isRoadBackwards = ( road->GetSourceIntersection() == static_cast<Intersection*>( pathElement->elem ) );

                                break;
                            }
                        }
                    }
                    else
                    {
                        isRoadBackwards = ( road->GetSourceIntersection() != previousIntersection );
                    }

                    int currentRoadSegmentIndex = isRoadBackwards ? numRoadSegments - 1 : 0;

                    while( currentRoadSegmentIndex >= 0 && currentRoadSegmentIndex < numRoadSegments )
                    {
                        RoadSegment* currentRoadSegment = road->GetRoadSegment( currentRoadSegmentIndex );
                        rmt::Vector vec0, vec1, vec2, vec3, start, end;

                        currentRoadSegment->GetCorner( 0, vec0 );
                        currentRoadSegment->GetCorner( 1, vec1 );
                        currentRoadSegment->GetCorner( 2, vec2 );
                        currentRoadSegment->GetCorner( 3, vec3 );

                        start = (vec0 + vec3) * 0.5f;
                        start.y = 0.0f;
                        end = (vec1 + vec2) * 0.5f;
                        end.y = 0.0f;

                        rmt::Vector intersectPoints[ 2 ];
                        int numIntersectPoints = IntersectLineSphere( start, end, visibleSphere, intersectPoints );
                        if( numIntersectPoints > 0 )
                        {
                            rmt::Vector closestIntersectPoint = intersectPoints[ 0 ];

                            // TC [TODO]: if more than one intersection points, need to determine which
                            //            one is closest to source
                            //
                            if( numIntersectPoints > 1 )
                            {
                                rTuneWarningMsg( false, "Multiple intersection points not yet handled properly, but this should not happen here!" );
                            }

                            // check to make sure that this point is, in fact, closer to the target than the source
                            //
                            if( i == 0 ) // only need to check this for the first path element
                            {
                                float roadSegT = roadManager->DetermineSegmentT( closestIntersectPoint, const_cast<RoadSegment*>( currentRoadSegment ) );
                                float intersectRoadT = roadManager->DetermineRoadT( const_cast<RoadSegment*>( currentRoadSegment ), roadSegT );

                                if( ( isRoadBackwards && intersectRoadT < sourceRoadT) ||
                                    (!isRoadBackwards && intersectRoadT > sourceRoadT) )
                                {
                                    target = closestIntersectPoint;
                                    isClosestPathElementFound = true;

                                    break; // stop iterating over road segments
                                }
                                else
                                {
                                    int dummy = 0;
                                }
                            }
                            else
                            {
                                target = closestIntersectPoint;
                                isClosestPathElementFound = true;

                                break; // stop iterating over road segments
                            }
                        }

                        currentRoadSegmentIndex += isRoadBackwards ? -1 : +1;
                    }

                    break;
                }
                case RoadManager::ET_INTERSECTION:
                {
                    Intersection* intersection = static_cast<Intersection*>( currentPathElement->elem );
                    rAssert( intersection != NULL );

                    // keep track of previous intersection
                    //
                    previousIntersection = intersection;

                    rmt::Vector intersectionLocation;
                    intersection->GetLocation( intersectionLocation );
                    intersectionLocation.y = 0.0f;
                    float distance = CalculateDistanceBetweenPoints( source, intersectionLocation );

                    if( rmt::Abs( distance - visibleRadius ) <= intersection->GetRadius() )
                    {
                        // project target onto radar edge, on a point that is closest to the
                        // intersection center point
                        //
                        rmt::Vector sourceToIntersection;
                        sourceToIntersection.Sub( intersectionLocation, source );
                        sourceToIntersection.Scale( visibleRadius / distance );

                        target.Add( source, sourceToIntersection );

                        isClosestPathElementFound = true;
                    }

                    break;
                }
                default:
                {
                    rAssertMsg( false, "Invalid path element type!" );

                    break;
                }
            }
        }
    }

    HeapMgr()->PopHeap( GMA_TEMP );

    target.y = 0.0f;
    m_lastOnRoadLocation = target;

#ifndef RAD_RELEASE
    if( !isClosestPathElementFound )
    {
        // TC: [INVESTIGATE] if closest path element is not found, there must be some discontinuity
        //                   in the closest path found
        //
        rTuneWarningMsg( false, "Closest path element not found! Please go tell Tony." );
    }
#endif
}

