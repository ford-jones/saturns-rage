/* =========================================
    Saturns Rage
    Ford Jones
    Jan 20 2025
    Lazarus v0.5.1
============================================ */

#include <lazarus.h>
#include <memory>
#include <random>
#include <string>
#include <cmath>

int shader  = 0;

bool game_over = false;

const int collision_damage = 13;
const int mouse_sensitivity = 5;
const int max_asteroids = 20;
const float collision_radius = 2.0;
const int health_bonus_frequency = 100;
const int max_health = 100;
const int max_ammo = 30;

int rand_offset_b = 0;
int rand_offset_a = 0;
int keycode_last_tick = 0;
float rotation_x_last_tick = 0.0;
float rotation_z_last_tick = 0.0;

std::unique_ptr<Lazarus::AudioManager>  audio_manager    = std::make_unique<Lazarus::AudioManager>();
std::unique_ptr<Lazarus::WindowManager> window           = nullptr;
std::unique_ptr<Lazarus::CameraManager> camera_manager   = nullptr;
std::unique_ptr<Lazarus::TextManager>   text_manager     = nullptr;
std::unique_ptr<Lazarus::LightManager>  light_manager    = nullptr;
std::unique_ptr<Lazarus::MeshManager>   mesh_manager     = nullptr;
std::unique_ptr<Lazarus::WorldFX>       world            = nullptr;

Lazarus::Shader                         shader_program;
Lazarus::Transform                      transformer;
Lazarus::EventManager                   event_manager;
Lazarus::GlobalsManager                 globals;

Lazarus::CameraManager::Camera          camera          = {};
Lazarus::LightManager::Light            light           = {};
Lazarus::WorldFX::SkyBox                skybox          = {};

int health_text_index = 0;
int asteroids_since_last_bonus = 0;

struct Asteroid
{
    int z_spawn_offset, y_spawn_offset;
    float z_rotation, y_rotation, movement_speed, scale;
    bool has_colided;

    Lazarus::MeshManager::Mesh mesh;
};

struct Health
{
    int modifier, x_spawn_offset, y_spawn_offset;
    bool has_colided;
    Lazarus::MeshManager::Mesh mesh;
};

struct Missile
{
    float y_spawn_offset, z_spawn_offset;
    bool is_travelling;
    Lazarus::MeshManager::Mesh mesh;
};

struct Spaceship
{
    int health, shield, ammo;
    float x_rotation;
    std::vector<Missile> missiles;
    Lazarus::MeshManager::Mesh mesh;
};

std::vector<Lazarus::AudioManager::Audio> samples = {};
std::vector<Missile> missiles = {};
std::vector<Asteroid> asteroids = {};
Spaceship spaceship = {};
Health health_bonus = {};

void generate_random_numbers()
{
    //  Retrieve a random number between -8 and +8
    rand_offset_b = (0 + (rand() % 16)) - 8;
    rand_offset_a = (0 + (rand() % 16)) - 8;
};

void init()
{   
    //  Engine settings
    globals.setEnforceImageSanity(true);
    globals.setMaxImageSize(500, 500);
    globals.setLaunchInFullscreen(true);
    globals.setCursorHidden(true);

    //  Construct window
    //  TODO: Fix the need to be init window prior to shader
    window = std::make_unique<Lazarus::WindowManager>("Saturns Rage");

    //  Initialise resources
    window->initialise();
    event_manager.initialise();
    audio_manager->initialise();
    shader = shader_program.initialiseShader();
    window->loadConfig(shader);

    //  Construct managers
    text_manager     = std::make_unique<Lazarus::TextManager>(shader);
    light_manager    = std::make_unique<Lazarus::LightManager>(shader);
    camera_manager   = std::make_unique<Lazarus::CameraManager>(shader);
    mesh_manager     = std::make_unique<Lazarus::MeshManager>(shader);
    world            = std::make_unique<Lazarus::WorldFX>(shader);

    //  Spaceship definition
    spaceship.mesh = mesh_manager->create3DAsset("assets/mesh/shuttle.obj", "assets/material/shuttle.mtl");
    spaceship.health = max_health;
    spaceship.ammo = max_ammo;
    spaceship.shield = 0;
    spaceship.x_rotation = 0.0;
    spaceship.missiles = {};
    transformer.translateMeshAsset(spaceship.mesh, -15.0, -1.0, 0.0);
    transformer.rotateMeshAsset(spaceship.mesh, 0.0, 180.0, 0.0);

    //  Asteroid(s) definition
    for(int i = 0; i < max_asteroids; i++)
    {
        Asteroid asteroid = {};
        asteroid.has_colided = false;
        asteroid.movement_speed = 0.08 + (static_cast<float>(i) / 100);
        asteroid.y_rotation = 0.0;
        asteroid.z_rotation = 0.0;

        generate_random_numbers();

        asteroid.y_spawn_offset = rand_offset_b;
        asteroid.z_spawn_offset = rand_offset_a;

        //  Add 8.0 to ensure a positively signed number, otherwise the meshes model matrix will invert 
        asteroid.scale = (rand_offset_a + 8.0f) / 4.0f;
        
        asteroid.mesh = mesh_manager->create3DAsset("assets/mesh/asteroid.obj", "assets/material/asteroid.mtl", "assets/images/rock.png");
        //  Start each asteroid at a different distance offset, so that they pass the respawn threshold at different times.
        transformer.translateMeshAsset(asteroid.mesh, -60.0 + (i * 5), asteroid.y_spawn_offset, asteroid.z_spawn_offset);  
        transformer.scaleMeshAsset(asteroid.mesh, asteroid.scale, asteroid.scale, asteroid.scale);

        asteroids.push_back(asteroid);
    };

    //  Health powerup definition
    generate_random_numbers();
    health_bonus.x_spawn_offset = rand_offset_a;
    health_bonus.y_spawn_offset = rand_offset_b;
    health_bonus.mesh = mesh_manager->createQuad(2.0, 2.0, "assets/images/health_icon.png");
    health_bonus.has_colided = false;
    health_bonus.modifier = 15;
    transformer.rotateMeshAsset(health_bonus.mesh, 0.0, 90.0, 0.0);
    transformer.translateMeshAsset(health_bonus.mesh, health_bonus.x_spawn_offset, health_bonus.y_spawn_offset, -60.0);

    // Missiles
    for(int i = 0; i < spaceship.ammo; i++)
    {
        Missile missile = {};
        missile.is_travelling = false;
        missile.y_spawn_offset = 0.0;
        missile.z_spawn_offset = 0.0;
        missile.mesh = mesh_manager->create3DAsset("assets/mesh/rocket.obj", "assets/material/rocket.mtl");
        transformer.translateMeshAsset(missile.mesh, -15.0, 0.0, 0.0);
        missiles.push_back(missile);
        spaceship.missiles.push_back(missile);
    };

    //  Spacial environment
    skybox  = world->createSkyBox("assets/skybox/right.png", "assets/skybox/left.png", "assets/skybox/bottom.png", "assets/skybox/top.png", "assets/skybox/front.png", "assets/skybox/back.png");
    light   = light_manager->createLightSource(-1.0, 1.0, 1.0, 1.0, 1.0, 1.0);
    camera  = camera_manager->createPerspectiveCam(0.0, -0.2, 0.0, 1.0, 0.0, 0.0);

    //  HUD
    text_manager->extendFontStack("assets/fonts/clock.ttf", 50);
    health_text_index = text_manager->loadText(std::string("Ship health: ").append(std::to_string(spaceship.health)), 0, 0, 10, 1.0f, 0.0f, 0.0f);

    //  Load audio
    samples.push_back(audio_manager->createAudio("assets/sound/crash1.mp3"));
    samples.push_back(audio_manager->createAudio("assets/sound/headswirler.wav", false, -1));
    for(int i = 0; i < samples.size(); i++)
    {
        audio_manager->loadAudio(samples[i]);
        audio_manager->pauseAudio(samples[i]);
    };
};

void load_environment()
{
    camera_manager->loadCamera(camera);
    light_manager->loadLightSource(light);

    world->drawSkyBox(skybox, camera);
};

void draw_assets()
{
    //  Draw spaceship
    mesh_manager->loadMesh(spaceship.mesh);
    mesh_manager->drawMesh(spaceship.mesh);

    //  Draw each asteroid
    for(int i = 0; i < asteroids.size(); i++)
    {
        mesh_manager->loadMesh(asteroids[i].mesh);
        mesh_manager->drawMesh(asteroids[i].mesh);
    };

    //  Draw health powerup
    //  Note: Only if it hasn't already been picked up
    if(!health_bonus.has_colided)
    {
        mesh_manager->loadMesh(health_bonus.mesh);
        mesh_manager->drawMesh(health_bonus.mesh);
    };

    //  Draw missiles
    for(int i = 0; i < missiles.size(); i++)
    {
        if(missiles[i].is_travelling)
        {
            mesh_manager->loadMesh(missiles[i].mesh);
            mesh_manager->drawMesh(missiles[i].mesh);
        };
    }

    //  Draw HUD
    //  Note: Text is drawn last to overlay
    text_manager->drawText(health_text_index);
};

int check_collisions(Lazarus::MeshManager::Mesh a, Lazarus::MeshManager::Mesh b)
{
        //  Find the distance between two points in a volume
        //  d = √((x2 – x1)² + (y2 – y1)² + (z2 – z1)²).

        float diff_x = (a.locationX - b.locationX);
        float diff_y = (a.locationY - b.locationY);
        float diff_z = (a.locationZ - b.locationZ);

        float power_x = diff_x * diff_x;
        float power_y = diff_y * diff_y;
        float power_z = diff_z * diff_z;

        float distance = std::sqrt((power_x + power_y + power_z));

        if(distance < collision_radius)
        {
            return 1;
        }
        else
        {
            return 0;
        };
};

void game_end()
{
    window->close();
};

void move_spaceship()
{    
    //  Get monitor dimensions
    float center_x = static_cast<float>(globals.getDisplayWidth() / 2);
    float center_y = static_cast<float>(globals.getDisplayHeight() / 2);

    //  Get mouse position
    int mouse_x = event_manager.mouseX;
    int mouse_y = event_manager.mouseY;

    //  Store the ship's rotation from previous iteration
    rotation_x_last_tick = spaceship.x_rotation;

    //  Move right
    if(mouse_x > (center_x) + mouse_sensitivity)
    {
        transformer.translateMeshAsset(spaceship.mesh, 0.0, 0.0, 0.1);   
        transformer.rotateMeshAsset(spaceship.mesh, 0.2, 0.0, 0.0);
        transformer.translateCameraAsset(camera, -0.01, 0.0, 0.0, 0.02);
        window->snapCursor(center_x, center_y);

        spaceship.x_rotation += 0.2;
    }
    //  Move left
    else if(mouse_x < (center_x) - mouse_sensitivity)
    {
        transformer.translateMeshAsset(spaceship.mesh, 0.0, 0.0, -0.1);
        transformer.rotateMeshAsset(spaceship.mesh, -0.2, 0.0, 0.0);
        transformer.translateCameraAsset(camera, 0.01, 0.0, 0.0, 0.02);
        window->snapCursor(center_x, center_y);

        spaceship.x_rotation -= 0.2;
    };

    //  Move up
    if(mouse_y > (center_y) + mouse_sensitivity)
    {
        transformer.translateMeshAsset(spaceship.mesh, 0.0, -0.1, 0.0);
        transformer.translateCameraAsset(camera, 0.0, 0.01, 0.0, 0.02);
        window->snapCursor(center_x, center_y);
    }
    //  Move down
    else if(mouse_y < (center_y) - mouse_sensitivity)
    {
        transformer.translateMeshAsset(spaceship.mesh, 0.0, 0.1, 0.0);
        transformer.translateCameraAsset(camera, 0.0, -0.01, 0.0, 0.02);
        window->snapCursor(center_x, center_y);
    };

    //  Check for update to the ships rotation since last iteration
    //  If no change, level out the ship
    if(rotation_x_last_tick == spaceship.x_rotation)
    {
        transformer.rotateMeshAsset(spaceship.mesh, -spaceship.x_rotation, 0.0, 0.0);

        spaceship.x_rotation = 0.0;
    };

};

void move_asteroids()
{
    for(int i = 0; i < asteroids.size(); i++)
    {
        //  Update asteroid position
        transformer.translateMeshAsset(asteroids[i].mesh, asteroids[i].movement_speed, 0.0, 0.0);

        //  Reset asteroid position
        if(asteroids[i].mesh.locationX > 0.0)
        {
            asteroids_since_last_bonus += 1;
            asteroids[i].has_colided = false;
            //  Invert sign of last rotate + translate to move back to origin
            transformer.rotateMeshAsset(asteroids[i].mesh, 0.0, -asteroids[i].y_rotation, -asteroids[i].z_rotation);
            transformer.translateMeshAsset(asteroids[i].mesh, -60.0, static_cast<float>(-asteroids[i].y_spawn_offset), static_cast<float>(-asteroids[i].z_spawn_offset));

            asteroids[i].z_rotation = 0.0;
            asteroids[i].y_rotation = 0.0;

            generate_random_numbers();

            //  Move asteroid to random offset from center
            transformer.translateMeshAsset(asteroids[i].mesh, 0.0, static_cast<float>(rand_offset_b), static_cast<float>(rand_offset_a));
            asteroids[i].z_spawn_offset = rand_offset_a;
            asteroids[i].y_spawn_offset = rand_offset_b;
        };

        int collision = 0;
        collision = check_collisions(spaceship.mesh, asteroids[i].mesh);

        //  Check collision result
        //  Note: Use has_colided so as not to clock 50+ collisions in a frame
        if((collision == 1) && !asteroids[i].has_colided)
        {
            asteroids[i].has_colided = true;

            //  Set crash1.mp3 back to begining and play
            audio_manager->setPlaybackCursor(samples[0], 1);
            audio_manager->playAudio(samples[0]);

            //  Bounce asteroid off ship
            generate_random_numbers();
            asteroids[i].z_rotation = rand_offset_a * 3;
            asteroids[i].y_rotation = rand_offset_b * 3;
            transformer.rotateMeshAsset(asteroids[i].mesh, 0.0, asteroids[i].y_rotation, asteroids[i].z_rotation);

            //  Update ship health
            spaceship.health -= collision_damage;
            text_manager->loadText(std::string("Ship health: ").append(std::to_string(spaceship.health)), 0, 0, 10, 1.0f, 0.0f, 0.0f, health_text_index);

            if(spaceship.health <= 0)
            {
                game_over = true;
            };
        };
    };
};

void move_background()
{
    transformer.rotateMeshAsset(skybox.cube, 0.0, -0.2, 0.0);
};

void move_health_powerup()
{
    int collision = check_collisions(spaceship.mesh, health_bonus.mesh);

    //  Health pickup being collected
    if(collision == 1 && !health_bonus.has_colided)
    {
        health_bonus.has_colided = true;

        //  Add 15 units of health up to a maximum of 100hp
        for(int i = 0; i < health_bonus.modifier; i++)
        {
            if(spaceship.health == 100) break;
            spaceship.health += 1;
        };
        
        //  Update HUD
        text_manager->loadText(std::string("Ship health: ").append(std::to_string(spaceship.health)), 0, 0, 10, 1.0f, 0.0f, 0.0f, health_text_index);

        //  TODO:
        //  Play some "rejuvination" sound on pickup
    }

    //  Health pickup has moved beyond camera viewport
    if(health_bonus.mesh.locationX > 0.0)
    { 
        health_bonus.has_colided = true;
    };

    //  If 100 asteroids have passed the camera viewport, reset the health pickup
    if(asteroids_since_last_bonus == health_bonus_frequency)
    {
        health_bonus.has_colided = false;
        transformer.translateMeshAsset(health_bonus.mesh, 0.0, 0.0, -60.0);
        asteroids_since_last_bonus = 0;

        transformer.translateMeshAsset(health_bonus.mesh, -static_cast<float>(-health_bonus.x_spawn_offset), static_cast<float>(-health_bonus.y_spawn_offset), 0.0);
        
        generate_random_numbers();
        //  Move health to random offset from center
        transformer.translateMeshAsset(health_bonus.mesh, static_cast<float>(rand_offset_a), static_cast<float>(rand_offset_b), 0.0);
        health_bonus.x_spawn_offset = rand_offset_a;
        health_bonus.y_spawn_offset = rand_offset_b;
    };
    
    //  Move health pickup.
    //  Note: 
    //  Travel on z because rotated 90deg
    //  Transforms INS are performed on an asset's local-coordinate system. 
    //  Notice the values printed by health_bonus.location* (i.e. OUTS) show where the object is in the world coordinate system.
    if(!health_bonus.has_colided) transformer.translateMeshAsset(health_bonus.mesh, 0.0, 0.0, 0.1);
};

void move_rockets()
{
    for(int i = 0; i < missiles.size(); i++)
    {
        //  Fire missiles with spacebar
        if(
            event_manager.keyCode == 32 && 
            (spaceship.ammo - 1) == i   && 
            event_manager.keyCode != keycode_last_tick
        ) 
        {
            spaceship.ammo -= 1;
            missiles[i].is_travelling = true;
            missiles[i].y_spawn_offset = spaceship.mesh.locationY;
            missiles[i].z_spawn_offset = spaceship.mesh.locationZ;
            transformer.translateMeshAsset(missiles[i].mesh, -1.0, missiles[i].y_spawn_offset, missiles[i].z_spawn_offset);
        };

        //  Advance traveling missiles
        if(missiles[i].is_travelling)
        {
            transformer.translateMeshAsset(missiles[i].mesh, -1.0, 0.0, 0.0);
        }

        //  Reset missiles which have reached the bounds
        if(missiles[i].mesh.locationX <= -60.0f)
        {
            missiles[i].is_travelling = false;
            transformer.translateMeshAsset(missiles[i].mesh, 60.0f, -missiles[i].y_spawn_offset, -missiles[i].z_spawn_offset);
        }
    };

    //  Retrieve latest keydown event
    //  Note: Used to control rate of fire
    keycode_last_tick = event_manager.keyCode;
};

int main()
{
    init();
    window->open();

    audio_manager->playAudio(samples[1]);

    while(window->isOpen)
    {
        std::cout << "asteroids since last bonus: " << asteroids_since_last_bonus << std::endl;
        event_manager.listen();

        //  Render scene
        load_environment();
        draw_assets();

        //  Do game mechanics
        move_spaceship();
        move_asteroids();
        move_health_powerup();
        move_background();
        move_rockets();

        std::cout << "Ammo: " << spaceship.ammo << std::endl;

        if
        (
            globals.getExecutionState() != LAZARUS_OK   || // If some error has surfaced from engine state
            event_manager.keyCode == 88                 || // Or the user hits the 'X' key
            game_over                                      // Or a win/lose condition has been met
        )
        {
            game_end();
        }
        else
        {
            window->handleBuffers();
        };
    };

    return 0;
};