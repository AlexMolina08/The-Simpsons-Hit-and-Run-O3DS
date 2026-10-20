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
#include <pddi/pica/bottomframe.hpp>
#include <presentation/gui/utility/handheldtext.h>
#include <presentation/gui/ingame/guimanageringame.h>
#include <presentation/gui/ingame/guiscreenhud.h>
#include <cheats/portsettings.h>
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
            if( label && label->GetFont() && label->IsVisible() ) rows[count++]=i;
        }
        return count;
    }

    Scrooby::Drawable* s_bottomActions[128];
    bool s_bottomActionVisible[128];
    unsigned s_bottomActionCount=0;
    void HideBottomAction(Scrooby::Drawable* item)
    {
        if( !item || s_bottomActionCount>=128 ) return;
        for( unsigned i=0;i<s_bottomActionCount;++i ) if(s_bottomActions[i]==item)return;
        s_bottomActions[s_bottomActionCount]=item;
        s_bottomActionVisible[s_bottomActionCount++]=item->IsVisible();
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
    CGuiScreen* screen=CurrentBottomScreen();
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

    if( PortSettings::MapOnTop() )
    {
        return;
    }
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

    void BottomLabel( tFont* font, const P3D_UNICODE* text, float x, float y,
                      float height, tColour colour, float maxWidth=230 )
    {
        if( text == NULL || font->GetFontHeight() <= 0 ) return;
        float scale=height/(320.0f*font->GetFontHeight());
        const float width=font->GetTextWidth(text);
        if( width*scale > maxWidth/320 ) scale=maxWidth/(320*width);
        const tColour original=font->GetColour();
        p3d::stack->Push();
        p3d::stack->Translate( (x+1)/320, (240-y-1)/320, 0 );
        p3d::stack->Scale( scale, scale, 1 );
        font->SetColour( tColour(8,6,14) );font->DisplayText( text );
        p3d::stack->Pop();
        p3d::stack->Push();
        p3d::stack->Translate( x/320, (240-y)/320, 0 );
        p3d::stack->Scale( scale, scale, 1 );
        font->SetColour( colour );font->DisplayText( text );
        p3d::stack->Pop();
        font->SetColour( original );
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
        const bool splash=id==CGuiWindow::GUI_SCREEN_ID_SPLASH;
        const bool movie=GetPresentationManager() && GetPresentationManager()->GetFMVPlayer() &&
            GetPresentationManager()->GetFMVPlayer()->IsPlaying();
        // Cinematics and boot artwork belong to the upper display only.
        if( view == NULL || screen == NULL || movie || (!loading && !tutorial && !stats && !screen->HasMenu()) )
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
        int selectedRow=-1;
        for( int i=0;i<rowCount;++i ) if(rows[i]==menu->GetSelection())selectedRow=i;
        const int first=selectedRow>=0 ? (selectedRow/5)*5 : 0;
        const int visibleRows=rowCount-first<5 ? rowCount-first : 5;
        if( !picaBottomScreenInterfaceBegin() ) return;
        view->BeginRender();
        if( mainMenu ) picaBottomScreenStatic();
        else picaBottomScreenPanel(-1,0);
        if( !startupLoading && !splash && (list || visibleRows>0) ) picaBottomScreenPanel(list ? selection : selectedRow-first,
            list ? count : visibleRows);
        p3d::pddi->PushState( PDDI_STATE_RENDER );
        p3d::pddi->PushState( PDDI_STATE_VIEW );
        p3d::pddi->EnableZBuffer(false);
        p3d::pddi->SetCamera(0.1f,10.0f,1.5708f,4.0f/3);
        p3d::pddi->SetProjectionMode(PDDI_PROJECTION_PERSPECTIVE);
        p3d::pddi->SetCullMode(PDDI_CULL_NONE);
        p3d::stack->Push();p3d::stack->LoadIdentity();
        p3d::stack->Translate(-0.5f,-0.375f,0.5f);
        if( startupLoading || splash )
        {
            Scrooby::Screen* resource=screen->GetScroobyScreen();
            Scrooby::Text* label=splash && menu && menu->GetNumItems()>0
                ? MenuLabel(menu->GetMenuItem(0)) : NULL;
            for( int i=0; resource && !label && i<resource->GetNumberOfPages(); ++i )
                label=resource->GetPageByIndex(i)->GetText("Loading");
            if( label && label->GetFont() )
            {
                tFont* font=label->GetFont();
                P3D_UNICODE loadingText[16];p3d::AsciiToUnicode("LOADING",loadingText,16);
                const P3D_UNICODE* text=splash ? label->GetStringBuffer() : loadingText;
                const float width=font->GetFontHeight()>0
                    ? font->GetTextWidth(text)*16/font->GetFontHeight() : 0;
                BottomLabel(font,text,(320-width)*0.5f,128,16,tColour(255,255,255));
            }
        }
        else if( list )
        {
            tFont* font=choices->GetFont();
            P3D_UNICODE hint[48];
            p3d::AsciiToUnicode("A: SELECT   /   TAP TWICE",hint,48);
            BottomLabel(font,hint,43,215,9,tColour(192,183,211));
            for( int i=0; i<count && i<choices->GetNumOfStrings(); ++i )
                BottomLabel(font,choices->GetStringBuffer(i),49,
                    picaBottomFrame::MenuTop(count)+i*picaBottomFrame::MenuRowHeight+17,
                    13,i==selection ? choices->GetColour() : tColour(225,219,234));
        }
        else if( visibleRows>0 )
        {
            tFont* hintFont=MenuLabel(menu->GetMenuItem(rows[first]))->GetFont();
            for( int row=0;row<visibleRows;++row )
            {
                const int index=rows[first+row];
                GuiMenuItem* item=menu->GetMenuItem(index);
                Scrooby::Text* label=MenuLabel(item);
                Scrooby::Text* value=dynamic_cast<Scrooby::Text*>(item->GetItemValue());
                const bool slider=item->m_slider.m_pPolygon || item->m_slider.m_pImage;
                const bool separate=value && value!=label;
                const float y=picaBottomFrame::MenuTop(visibleRows)+row*picaBottomFrame::MenuRowHeight+17;
                const tColour colour=!menu->IsMenuItemEnabled(index) ? tColour(128,128,128) :
                    first+row==selectedRow ? tColour(255,235,0) : tColour(225,219,234);
                const bool single=visibleRows==1 && !separate && !slider;
                const float height=single ? 16.0f : 13.0f;
                float x=49;
                if( single && label->GetFont()->GetFontHeight()>0 )
                {
                    float width=label->GetFont()->GetTextWidth(label->GetStringBuffer())*
                        height/label->GetFont()->GetFontHeight();
                    if(width>230)width=230;
                    x=(320-width)*0.5f;
                }
                BottomLabel(label->GetFont(),label->GetStringBuffer(),x,y,height,colour,
                    separate || slider ? 142 : 230);
                if( separate && value->GetFont() )
                    BottomLabel(value->GetFont(),value->GetStringBuffer(),202,y,12,colour,78);
                else if( slider )
                {
                    char percent[16];sprintf(percent,"%d%%",int(item->m_slider.m_value*100+0.5f));
                    P3D_UNICODE number[16];p3d::AsciiToUnicode(percent,number,16);
                    BottomLabel(label->GetFont(),number,222,y,12,colour,58);
                }
            }
            P3D_UNICODE hint[32];
            p3d::AsciiToUnicode(screen->IsButtonVisible(BUTTON_ICON_BACK) ? "A SELECT   /   B BACK" : "A SELECT",hint,32);
            BottomLabel(hintFont,hint,43,215,9,tColour(192,183,211));
        }
        else if( stats )
        {
            Scrooby::Page* page=screen->GetScroobyScreen()->GetPage("LevelStats");
            Scrooby::Group* group=page ? page->GetGroup("Stats") : NULL;
            Scrooby::Text* heading=page ? page->GetText("LevelProgress") : NULL;
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
                    BottomLabel(font,title,(320-width)*0.5f,23,16,tColour(255,235,0),260);
                }
                for( unsigned row=0;row<9;++row )
                {
                    Scrooby::Text* label=group->GetText(names[row][0]);
                    Scrooby::Text* value=group->GetText(names[row][1]);
                    if( !label || !value ) continue;
                    const float y=46+row*18;
                    const tColour colour=row==8 ? tColour(255,235,0) : tColour(225,219,234);
                    BottomLabel(font,label->GetStringBuffer(),30,y,15,colour,184);
                    const P3D_UNICODE* number=value->GetStringBuffer();
                    float width=font->GetTextWidth(number)*15/font->GetFontHeight();
                    if( width>64 ) width=64;
                    BottomLabel(font,number,290-width,y,15,colour,64);
                }
                P3D_UNICODE hint[32];
                p3d::AsciiToUnicode("A CONTINUE   /   B BACK",hint,32);
                BottomLabel(font,hint,30,220,12,tColour(225,219,234),260);
            }
        }
        else if( id==CGuiWindow::GUI_SCREEN_ID_LOADING )
        {
            Scrooby::Page* page=screen->GetScroobyScreen()->GetPage("Loading");
            Scrooby::Text* label=page ? page->GetText("Loading") : NULL;
            if( label && label->GetFont() )
            {
                tFont* font=label->GetFont();
                P3D_UNICODE title[32],hint[32];
                p3d::AsciiToUnicode("SPRINGFIELD SHOPPER",title,32);
                p3d::AsciiToUnicode("PLEASE STAND BY",hint,32);
                const float titleWidth=font->GetTextWidth(title)*18/font->GetFontHeight();
                const float hintWidth=font->GetTextWidth(hint)*12/font->GetFontHeight();
                BottomLabel(font,title,(320-titleWidth)*0.5f,76,18,tColour(255,235,0),280);
                BottomLabel(font,hint,(320-hintWidth)*0.5f,108,12,tColour(225,219,234),280);
                const P3D_UNICODE* text=label->GetStringBuffer();
                const float width=font->GetTextWidth(text)*16/font->GetFontHeight();
                BottomLabel(font,text,(320-width)*0.5f,175,16,tColour(255,255,255),280);
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
        p3d::stack->Pop();
        p3d::pddi->PopState(PDDI_STATE_VIEW);
        p3d::pddi->PopState(PDDI_STATE_RENDER);
        view->EndRender();
        if( mainMenu ) picaBottomScreenFrame();
        picaBottomScreenEnd();
    }
}

void CHudMap::AfterRenderLayers3DS( tView* guiView )
{
    for( unsigned i=0;i<s_bottomDialogCount;++i )
        s_bottomDialogPages[i]->SetVisible(s_bottomDialogVisible[i]);
    for( unsigned i=0;i<s_bottomActionCount;++i )
        s_bottomActions[i]->SetVisible(s_bottomActionVisible[i]);
    CHudMap* map = s_bottomMap;
    if( map != NULL )
    {
        map->m_hudMapGroup->SetVisible( s_mapWasVisible );
        if( s_bottomBackground != NULL )
        {
            s_bottomBackground->SetVisible( s_backgroundWasVisible );
        }
    }

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
    if( map == NULL )
    {
        if( picaBottomScreenBegin(0,0,1,1) )
        {
            picaBottomScreenPanel(-1,0);picaBottomScreenEnd();
        }
        return;
    }

    CGuiManagerInGame* inGame = GetGuiSystem()->GetInGameManager();
    if( guiView == NULL || inGame == NULL || inGame->GetCurrentScreen() != CGuiWindow::GUI_SCREEN_ID_HUD )
    {
        DisplayBottomInterface(guiView);
        return;
    }

    const bool shown = s_mapWasVisible &&
                       ( map->m_hudLayer == NULL || map->m_hudLayer->IsVisible() );
    if( !shown )
    {
        if( picaBottomScreenBegin(0,0,1,1) )
        {
            picaBottomScreenPanel(-1,0);picaBottomScreenEnd();
        }
        return;
    }

#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
    // Visibility restoration and all simulation/UI updates already ran. Retain
    // the displayed lower image between map draws; menus and transitions reset.
    if(!Old3dsBottomMap::Due())return;
#endif
    float rect[ 4 ];
    map->GetBottomScreenSource( rect );
    if( picaBottomScreenBegin( rect[ 0 ], rect[ 1 ], rect[ 2 ], rect[ 3 ] ) )
    {
        map->DisplayOnBottomScreen( guiView, s_backgroundWasVisible ? s_bottomBackground : NULL );
        picaBottomScreenEnd();
#ifdef SRR2_OLD3DS_BOTTOM_MAP_15HZ
        Old3dsBottomMap::Rendered();
#endif
    }
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
    const float half = 0.44f * ( y1 - y0 );

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
    guiView->BeginRender();

    const bool wide = p3d::display->IsWidescreen();
    if( wide )
    {
        m_p3dMap->SetWideScreenCorrectionEnabled( false );
        if( m_p3dHole != NULL )
        {
            m_p3dHole->SetWideScreenCorrectionEnabled( false );
        }
    }

    p3d::pddi->PushState( PDDI_STATE_RENDER );
    p3d::pddi->PushState( PDDI_STATE_VIEW );
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
    p3d::pddi->PopState( PDDI_STATE_RENDER );
    p3d::pddi->PopState( PDDI_STATE_VIEW );

    guiView->EndRender();

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

