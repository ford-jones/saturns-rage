/* =========================================
    Saturns Rage
    Ford Jones
    Jan 24 2025
    Lazarus v0.5.2
============================================ */

#include <lazarus.h>
#include <memory>
#include <random>
#include <string>
#include <cmath>

int shader  = 0;

bool game_over = false;

const float collision_radius        = 2.0;
const int   base_collision_damage   = 10;
const int   mouse_sensitivity       = 5;
const int   starting_asteroids      = 20;
const int   health_bonus_frequency  = 100;
const int   max_health              = 100;
const int   max_ammo                = 30;

int     frame_count             = 0;
int     rand_offset_b           = 0;
int     rand_offset_a           = 0;
int     keycode_last_tick       = 0;
float   rotation_x_last_tick    = 0.0;
float   rotation_z_last_tick    = 0.0;
float   brightness_last_tick    = 0.0;

struct Asteroid
{
    int damage_modifier, z_spawn_offset, y_spawn_offset, points_worth;
    float z_rotation, y_rotation, movement_speed, scale;
    bool has_colided, is_fragment, exploded;

    Lazarus::MeshManager::Mesh mesh;
};

struct PowerUp
{
    int modifier, x_spawn_offset, y_spawn_offset, appearance_frequency, asteroid_counter, type; 
    bool has_colided;
    Lazarus::MeshManager::Mesh mesh;
};

struct Missile
{
    float y_spawn_offset, z_spawn_offset;
    bool is_travelling, has_colided;
    Lazarus::MeshManager::Mesh mesh;
    Lazarus::LightManager::Light explosion;
};

struct Spaceship
{
    int health, shield, ammo;
    float x_rotation;
    std::vector<Missile> missiles;
    Lazarus::MeshManager::Mesh mesh;
};

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
Lazarus::LightManager::Light            point_light     = {};
Lazarus::WorldFX::SkyBox                skybox          = {};
Lazarus::MeshManager::Mesh              saturn_planet   = {};
Lazarus::MeshManager::Mesh              saturn_ring     = {};

int player_points = 0;

int health_text_index = 0;
int ammo_text_index = 0;
int points_text_index = 0;

std::vector<Lazarus::AudioManager::Audio> samples = {};
std::vector<Missile> missiles = {};
std::vector<Asteroid> asteroids = {};
Spaceship spaceship = {};
PowerUp health_bonus = {};
PowerUp ammo_bonus = {};

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
    for(int i = 0; i < starting_asteroids; i++)
    {
        Asteroid asteroid = {};
        asteroid.has_colided = false;
        asteroid.is_fragment = false;
        asteroid.exploded = false;
        asteroid.movement_speed = 0.08 + (static_cast<float>(i) / 100);
        asteroid.y_rotation = 0.0;
        asteroid.z_rotation = 0.0;

        generate_random_numbers();

        asteroid.y_spawn_offset = rand_offset_b;
        asteroid.z_spawn_offset = rand_offset_a;

        //  Add 8.0 to ensure a positively signed number, otherwise the meshes model matrix will invert 
        //  Note: Between +2.0 and +4.0
        asteroid.scale = (rand_offset_a + 8.0f) / 4.0f;
        asteroid.damage_modifier = base_collision_damage + asteroid.scale;
        asteroid.points_worth = ceil(asteroid.scale * 8.0);
        
        asteroid.mesh = mesh_manager->create3DAsset("assets/mesh/asteroid.obj", "assets/material/asteroid.mtl", "assets/images/rock.png");
        //  Start each asteroid at a different distance offset, so that they pass the respawn threshold at different times.
        transformer.translateMeshAsset(asteroid.mesh, -60.0 + (i * 5), asteroid.y_spawn_offset, asteroid.z_spawn_offset);  
        transformer.scaleMeshAsset(asteroid.mesh, asteroid.scale, asteroid.scale, asteroid.scale);

        asteroids.push_back(asteroid);
    };

    //  Health powerup definition
    generate_random_numbers();
    health_bonus.type = 1;
    health_bonus.x_spawn_offset = rand_offset_a;
    health_bonus.y_spawn_offset = rand_offset_b;
    health_bonus.mesh = mesh_manager->createQuad(2.0, 2.0, "assets/images/health_icon.png");
    health_bonus.appearance_frequency = 80;
    health_bonus.asteroid_counter = 0;
    health_bonus.has_colided = false;
    health_bonus.modifier = 20;
    transformer.rotateMeshAsset(health_bonus.mesh, 0.0, 90.0, 0.0);
    transformer.translateMeshAsset(health_bonus.mesh, health_bonus.x_spawn_offset, health_bonus.y_spawn_offset, -80.0);

    //  Ammo powerup definition
    generate_random_numbers();
    ammo_bonus.type = 2;
    ammo_bonus.x_spawn_offset = rand_offset_a;
    ammo_bonus.y_spawn_offset = rand_offset_b;
    ammo_bonus.mesh = mesh_manager->createQuad(2.0, 2.0, "assets/images/ammo_icon.png");
    ammo_bonus.appearance_frequency = 150;
    ammo_bonus.asteroid_counter = 0;
    ammo_bonus.has_colided = false;
    ammo_bonus.modifier = max_ammo;
    transformer.rotateMeshAsset(ammo_bonus.mesh, 0.0, 90.0, 0.0);
    transformer.translateMeshAsset(ammo_bonus.mesh, ammo_bonus.x_spawn_offset, ammo_bonus.y_spawn_offset, -60.0);

    // Missiles
    for(int i = 0; i < spaceship.ammo; i++)
    {
        Missile missile = {};
        missile.is_travelling = false;
        missile.has_colided = false;
        missile.y_spawn_offset = 0.0;
        missile.z_spawn_offset = 0.0;
        missile.explosion = light_manager->createLightSource(-1.0, 0.0, 0.0, 0.9, 0.5, 0.0, 0.0);
        missile.mesh = mesh_manager->create3DAsset("assets/mesh/rocket.obj", "assets/material/rocket.mtl");
        transformer.translateMeshAsset(missile.mesh, -15.0, 0.0, 0.0);
        missiles.push_back(missile);
        spaceship.missiles.push_back(missile);
    };

    //  Spacial environment
    point_light = light_manager->createLightSource(-8.5, 0.0, 0.0, 1.0, 1.0, 1.0);
    camera  = camera_manager->createPerspectiveCam(0.0, -0.2, 0.0, 1.0, 0.0, 0.0);
    skybox  = world->createSkyBox("assets/skybox/right.png", "assets/skybox/left.png", "assets/skybox/bottom.png", "assets/skybox/top.png", "assets/skybox/front.png", "assets/skybox/back.png");
    saturn_planet = mesh_manager->create3DAsset("assets/mesh/saturn_planet.obj", "assets/material/saturn_planet.mtl", "assets/images/planet.png");
    saturn_ring = mesh_manager->create3DAsset("assets/mesh/saturn_ring.obj", "assets/material/saturn_ring.mtl", "assets/images/ring.png");
    transformer.translateMeshAsset(saturn_planet, -60.0, 2.0, -15.0);
    transformer.translateMeshAsset(saturn_ring, -60.0, 2.0, -15.0);
    transformer.rotateMeshAsset(saturn_planet, 20.0, 0.0, -20.0);
    transformer.rotateMeshAsset(saturn_ring, 20.0, 0.0, -20.0);
    transformer.scaleMeshAsset(saturn_planet, 2.0, 2.0, 2.0);
    transformer.scaleMeshAsset(saturn_ring, 2.0, 2.0, 2.0);

    //  HUD
    text_manager->extendFontStack("assets/fonts/clock.ttf", 50);
    health_text_index = text_manager->loadText(std::string("SHIP HEALTH: ").append(std::to_string(spaceship.health)), 0, 0, 10, 1.0f, 0.0f, 0.0f);
    ammo_text_index = text_manager->loadText(std::string("AMMO: ").append(std::to_string(spaceship.ammo)), (globals.getDisplayWidth() - 330), 0, 10, 0.9f, 0.5f, 0.0f);
    points_text_index = text_manager->loadText(std::string("SCORE: ").append(std::to_string(player_points)), 0, (globals.getDisplayHeight() - 50), 10, 0.9f, 0.5f, 0.0f);

    //  Load audio
    samples.push_back(audio_manager->createAudio("assets/sound/crash1.mp3"));
    samples.push_back(audio_manager->createAudio("assets/sound/headswirler.wav", false, -1));
    for(int i = 0; i < samples.size(); i++)
    {
        audio_manager->loadAudio(samples[i]);
        audio_manager->pauseAudio(samples[i]);
    };
};

void fracture_asteroid(Asteroid parent)
{
    int target_size = asteroids.size() + 3;

    //  Dont repeat if fragments are getting too small
    if(!((parent.scale / 2.0) < 0.5))
    {
        while(asteroids.size() < target_size)
        {
            //  Create 3 new asteroids and add them to the container
            Asteroid asteroid = {};
            asteroid.has_colided = false;
            asteroid.is_fragment = true;
            asteroid.exploded = false;
            asteroid.movement_speed = parent.movement_speed * 1.5;

            generate_random_numbers();
            asteroid.y_rotation = rand_offset_a * 3.0;
            asteroid.z_rotation = rand_offset_b * 3.0;

            asteroid.y_spawn_offset = 0.0;
            asteroid.z_spawn_offset = 0.0;

            //  Make fragment 2x smaller than parent
            asteroid.scale = parent.scale / 2.0;
            asteroid.damage_modifier = floor(base_collision_damage + asteroid.scale);
            asteroid.points_worth = ceil(asteroid.scale * 8.0);

            //  Copy parent's mesh, including it's current location
            asteroid.mesh = parent.mesh;
            transformer.scaleMeshAsset(asteroid.mesh, asteroid.scale, asteroid.scale, asteroid.scale);
            transformer.rotateMeshAsset(asteroid.mesh, 0.0, asteroid.y_rotation, asteroid.z_rotation);


            asteroids.push_back(asteroid);
        };
    }
};

void load_environment()
{
    camera_manager->loadCamera(camera);
    light_manager->loadLightSource(point_light);

    for(int i = 0; i < missiles.size(); i++)
    {
        light_manager->loadLightSource(missiles[i].explosion);
    };

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
        if(!asteroids[i].exploded)
        {
            mesh_manager->loadMesh(asteroids[i].mesh);
            mesh_manager->drawMesh(asteroids[i].mesh);
        }
    };

    //  Draw health powerup
    //  Note: Only if it hasn't already been picked up
    if(!health_bonus.has_colided)
    {
        mesh_manager->loadMesh(health_bonus.mesh);
        mesh_manager->drawMesh(health_bonus.mesh);
    };

    //  Draw ammo powerup
    if(!ammo_bonus.has_colided)
    {
        mesh_manager->loadMesh(ammo_bonus.mesh);
        mesh_manager->drawMesh(ammo_bonus.mesh);
    };

    //  Draw missiles
    for(int i = 0; i < missiles.size(); i++)
    {
        if(missiles[i].is_travelling)
        {
            mesh_manager->loadMesh(missiles[i].mesh);
            mesh_manager->drawMesh(missiles[i].mesh);
        };
    };

    mesh_manager->loadMesh(saturn_planet);
    mesh_manager->drawMesh(saturn_planet);
    mesh_manager->loadMesh(saturn_ring);
    mesh_manager->drawMesh(saturn_ring);

    //  Draw HUD
    //  Note: Text is drawn last to overlay
    text_manager->loadText(std::string("SHIP HEALTH: ").append(std::to_string(spaceship.health)), 0, 0, 10, 1.0f, 0.0f, 0.0f, health_text_index);
    text_manager->drawText(health_text_index);
    text_manager->loadText(std::string("AMMO: ").append(std::to_string(spaceship.ammo)), (globals.getDisplayWidth() - 330), 0, 10, 0.9f, 0.5f, 0.0f, ammo_text_index);
    text_manager->drawText(ammo_text_index);
    text_manager->loadText(std::string("SCORE: ").append(std::to_string(player_points)), 0, (globals.getDisplayHeight() - 50), 10, 0.9f, 0.5f, 0.0f, points_text_index);
    text_manager->drawText(points_text_index);
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
        transformer.translateLightAsset(point_light, 0.2, 0.0, 0.0);
        transformer.translateCameraAsset(camera, -0.01, 0.0, 0.0, 0.01);
        transformer.translateMeshAsset(spaceship.mesh, 0.0, 0.0, 0.1);
        transformer.rotateMeshAsset(spaceship.mesh, 0.2, 0.0, 0.0);
        window->snapCursor(center_x, center_y);

        spaceship.x_rotation += 0.2;
    }
    //  Move left
    else if(mouse_x < (center_x) - mouse_sensitivity)
    {
        transformer.translateLightAsset(point_light, -0.2, 0.0, 0.0);
        transformer.translateCameraAsset(camera, 0.01, 0.0, 0.0, 0.01);
        transformer.translateMeshAsset(spaceship.mesh, 0.0, 0.0, -0.1);
        transformer.rotateMeshAsset(spaceship.mesh, -0.2, 0.0, 0.0);
        window->snapCursor(center_x, center_y);

        spaceship.x_rotation -= 0.2;
    };

    //  Move up
    if(mouse_y > (center_y) + mouse_sensitivity)
    {
        transformer.translateLightAsset(point_light, 0.0, -0.2, 0.0);
        transformer.translateCameraAsset(camera, 0.0, 0.01, 0.0, 0.01);
        transformer.translateMeshAsset(spaceship.mesh, 0.0, -0.1, 0.0);
        window->snapCursor(center_x, center_y);
    }
    //  Move down
    else if(mouse_y < (center_y) - mouse_sensitivity)
    {
        transformer.translateLightAsset(point_light, 0.0, 0.2, 0.0);
        transformer.translateCameraAsset(camera, 0.0, -0.01, 0.0, 0.01);
        transformer.translateMeshAsset(spaceship.mesh, 0.0, 0.1, 0.0);
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
            //  Kick child asteroids out
            if(asteroids[i].is_fragment)
            {
                int current_size = asteroids.size();
                asteroids.resize(current_size - 1);
            }
            else
            {
                health_bonus.asteroid_counter += 1;
                ammo_bonus.asteroid_counter += 1;
                asteroids[i].has_colided = false;
                asteroids[i].exploded = false;
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
            }

        };

        int collision = 0;
        
        if(!asteroids[i].exploded)
        {
            collision = check_collisions(spaceship.mesh, asteroids[i].mesh);
        };

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
            asteroids[i].z_rotation = rand_offset_a * 3.0;
            asteroids[i].y_rotation = rand_offset_b * 3.0;
            transformer.rotateMeshAsset(asteroids[i].mesh, 0.0, asteroids[i].y_rotation, asteroids[i].z_rotation);

            //  Update SHIP HEALTH
            spaceship.health -= asteroids[i].damage_modifier;

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

void move_powerup(PowerUp &powerup)
{
    int collision = check_collisions(spaceship.mesh, powerup.mesh);

    //  Powerup being collected
    if(collision == 1 && !powerup.has_colided)
    {
        powerup.has_colided = true;

        //  Health  = 1
        //  Ammo    = 2
        if(powerup.type == 1)
        {
            int target_health = spaceship.health + powerup.modifier;
            while((spaceship.health < target_health) && spaceship.health != max_health)
            {
                spaceship.health += 1;
            }
        }
        else
        {
            int target_ammo = spaceship.ammo + powerup.modifier;
            while((spaceship.ammo < target_ammo) && spaceship.ammo != max_ammo)
            {
                spaceship.ammo += 1;
            }
        };

        //  TODO:
        //  Play some "rejuvination" sound on pickup
    }

    //  Powerup has moved beyond camera viewport
    if(powerup.mesh.locationX > 0.0)
    { 
        powerup.has_colided = true;
    };

    //  Reset
    if(powerup.asteroid_counter == powerup.appearance_frequency)
    {
        powerup.has_colided = false;
        transformer.translateMeshAsset(powerup.mesh, 0.0, 0.0, -60);

        //  Set counter back on ammo collect
        powerup.asteroid_counter = 0;

        transformer.translateMeshAsset(powerup.mesh, -static_cast<float>(-powerup.x_spawn_offset), static_cast<float>(-powerup.y_spawn_offset), 0.0);
        
        generate_random_numbers();
        //  Move health to random offset from center
        transformer.translateMeshAsset(powerup.mesh, static_cast<float>(rand_offset_a), static_cast<float>(rand_offset_b), 0.0);
        powerup.x_spawn_offset = rand_offset_a;
        powerup.y_spawn_offset = rand_offset_b;
    };
    
    //  Move powerup.
    //  Note: 
    //  Travel on z because rotated 90deg
    //  Transforms INS are performed on an asset's local-coordinate system. 
    //  Notice the values printed by powerup.location* (i.e. OUTS) show where the object is in the world coordinate system.
    if(!powerup.has_colided) transformer.translateMeshAsset(powerup.mesh, 0.0, 0.0, 0.1);
};

void move_rockets()
{
    if(frame_count < 60)
    {
        frame_count += 1;
    }
    else
    {
        frame_count = 0;
    };
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

            //  Hold explosion glow for 30 frames
            if(frame_count >= 60 && brightness_last_tick > 0.0)
            {
                missiles[i].explosion.brightness = 0.0;
                missiles[i].explosion.locationX -= missiles[i].explosion.locationX;
                missiles[i].explosion.locationY -= missiles[i].explosion.locationY;
                missiles[i].explosion.locationZ -= missiles[i].explosion.locationZ;
            };

            //  Check collisions
            for(int j = 0; j < asteroids.size(); j++)
            {
                int colided = check_collisions(missiles[i].mesh, asteroids[j].mesh);
                // missiles[i].explosion.brightness = 0.0;
                if(colided == 1 && !asteroids[j].exploded && !missiles[i].has_colided)
                {
                    player_points += asteroids[j].points_worth;
                    transformer.translateLightAsset(missiles[i].explosion, missiles[i].mesh.locationX, missiles[i].mesh.locationY, missiles[i].mesh.locationZ);
                    asteroids[j].exploded = true;
                    missiles[i].has_colided = true;
                    //  TODO:
                    //  Play audio 
                    missiles[i].explosion.brightness = 3.0;
                    brightness_last_tick = missiles[i].explosion.brightness;
                    fracture_asteroid(asteroids[j]);
                };
            };
        }

        //  Reset missiles which have reached the bounds
        if(missiles[i].mesh.locationX <= -60.0f)
        {
            missiles[i].is_travelling = false;
            missiles[i].has_colided = false;
            missiles[i].explosion.brightness = 0.0;
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
        event_manager.listen();

        std::cout << "Points: " << player_points << std::endl;

        //  Render scene
        load_environment();
        draw_assets();

        //  Do game mechanics
        move_spaceship();
        move_asteroids();
        move_powerup(ammo_bonus);
        move_powerup(health_bonus);
        move_background();
        move_rockets();

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