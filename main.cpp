/* =========================================
    Saturns Rage
    Ford Jones
    Jan 15 2025
    Lazarus v0.4.4
============================================ */

#include <lazarus.h>
#include <memory>
#include <random>
#include <string>
#include <cmath>

int shader  = 0;

bool game_over = false;
int  ship_health = 100;
int  ship_shield = 0;

const int collision_damage = 13;
const int mouse_sensitivity = 5;
const int max_asteroids = 15;
const float collision_radius = 1.4;

int rand_offset_y = 0;
int rand_offset_z = 0;

std::unique_ptr<Lazarus::WindowManager> window           = nullptr;
std::unique_ptr<Lazarus::CameraManager> camera_manager   = nullptr;
std::unique_ptr<Lazarus::TextManager>   text_manager     = nullptr;
std::unique_ptr<Lazarus::LightManager>  light_manager    = nullptr;
std::unique_ptr<Lazarus::MeshManager>   mesh_manager     = nullptr;
// std::unique_ptr<Lazarus::AudioManager>  audio_manager    = nullptr;
std::unique_ptr<Lazarus::WorldFX>       world            = nullptr;

Lazarus::Shader                         shader_program;
Lazarus::Transform                      transformer;
Lazarus::EventManager                   event_manager;
Lazarus::GlobalsManager                 globals;

Lazarus::CameraManager::Camera          camera          = {};
Lazarus::LightManager::Light            light           = {};
Lazarus::MeshManager::Mesh              spaceship       = {};
Lazarus::WorldFX::SkyBox                skybox          = {};

int healthText = 0;

struct Asteroid
{
    int z_spawn_offset, y_spawn_offset;
    float z_rotation, y_rotation, movement_speed;
    bool has_colided;

    Lazarus::MeshManager::Mesh mesh;
};

std::vector<Asteroid>                   asteroids       = {};

void generate_random_numbers()
{
    rand_offset_y = (0 + (rand() % 16)) - 8;
    rand_offset_z = (0 + (rand() % 16)) - 8;
};

void init()
{   
    globals.setEnforceImageSanity(true);
    globals.setMaxImageSize(500, 500);

    globals.setLaunchInFullscreen(true);
    globals.setCursorHidden(true);

    window = std::make_unique<Lazarus::WindowManager>("Saturns Rage");

    window->initialise();
    event_manager.initialise();

    shader = shader_program.initialiseShader();

    text_manager = std::make_unique<Lazarus::TextManager>(shader);
    light_manager    = std::make_unique<Lazarus::LightManager>(shader);
    camera_manager   = std::make_unique<Lazarus::CameraManager>(shader);
    mesh_manager     = std::make_unique<Lazarus::MeshManager>(shader);
    world            = std::make_unique<Lazarus::WorldFX>(shader);
    // audio_manager = std::make_unique<Lazarus::AudioManager>();

    // audio_manager->initialise();

    window->loadConfig(shader);

    text_manager->extendFontStack("assets/fonts/clock.ttf", 50);

    light   = light_manager->createLightSource(0.0, 0.0, 0.0, 1.0, 1.0, 1.0);
    camera  = camera_manager->createPerspectiveCam(0.0, -0.2, 0.0, 1.0, 0.0, 0.0);

    for(int i = 0; i < max_asteroids; i++)
    {
        Asteroid asteroid;
        asteroid.has_colided = false;
        asteroid.movement_speed = 0.08 + (static_cast<float>(i) / 100);
        asteroid.y_rotation = 0.0;
        asteroid.z_rotation = 0.0;

        generate_random_numbers();

        asteroid.y_spawn_offset = rand_offset_y;
        asteroid.z_spawn_offset = rand_offset_z;
        
        asteroid.mesh = mesh_manager->create3DAsset("assets/mesh/asteroid.obj", "assets/material/asteroid.mtl", "assets/images/rock.png");
        transformer.translateMeshAsset(asteroid.mesh, -60.0 + (i * 5), asteroid.y_spawn_offset, asteroid.z_spawn_offset);  //  Start each asteroid at a different distance offset, so that they pass the respawn threshold at different times.

        asteroids.push_back(asteroid);
    };

    spaceship = mesh_manager->create3DAsset("assets/mesh/shuttle.obj", "assets/material/shuttle.mtl");
    
    transformer.translateMeshAsset(spaceship, -15.0, -1.0, 0.0);
    transformer.rotateMeshAsset(spaceship, 0.0, 180.0, 0.0);

    skybox = world->createSkyBox("assets/skybox/right.png", "assets/skybox/left.png", "assets/skybox/bottom.png", "assets/skybox/top.png", "assets/skybox/front.png", "assets/skybox/back.png");

    std::string hp = std::string("Ship health: ").append(std::to_string(ship_health));
    healthText = text_manager->loadText(hp, 0, 0, 10, 1.0f, 0.0f, 0.0f);
};

void process_user_inputs()
{    
    float center_x = static_cast<float>(globals.getDisplayWidth() / 2);
    float center_y = static_cast<float>(globals.getDisplayHeight() / 2);

    int mouse_x = event_manager.mouseX;
    int mouse_y = event_manager.mouseY;

    if(mouse_x > (center_x) + mouse_sensitivity)
    {
        transformer.translateMeshAsset(spaceship, 0.0, 0.0, 0.1);
        
        window->snapCursor(center_x, center_y);
    }
    else if(mouse_x < (center_x) - mouse_sensitivity)
    {
        transformer.translateMeshAsset(spaceship, 0.0, 0.0, -0.1);
        window->snapCursor(center_x, center_y);
    };

    if(mouse_y > (center_y) + mouse_sensitivity)
    {
        transformer.translateMeshAsset(spaceship, 0.0, -0.1, 0.0);
        window->snapCursor(center_x, center_y);
    }
    else if(mouse_y < (center_y) - mouse_sensitivity)
    {
        transformer.translateMeshAsset(spaceship, 0.0, 0.1, 0.0);
        window->snapCursor(center_x, center_y);
    };

    if(event_manager.keyCode == 88)
    {
        game_over = true;
    };
};

void draw_assets()
{
    world->drawSkyBox(skybox, camera);

    mesh_manager->loadMesh(spaceship);
    mesh_manager->drawMesh(spaceship);

    for(int i = 0; i < asteroids.size(); i++)
    {
        mesh_manager->loadMesh(asteroids[i].mesh);
        mesh_manager->drawMesh(asteroids[i].mesh);
    };

    text_manager->drawText(healthText);
};

int check_collisions(Lazarus::MeshManager::Mesh a, Lazarus::MeshManager::Mesh b)
{
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

void move_asteroids()
{
    for(int i = 0; i < asteroids.size(); i++)
    {
        transformer.translateMeshAsset(asteroids[i].mesh, asteroids[i].movement_speed, 0.0, 0.0);

        if(asteroids[i].mesh.locationX > 0.0)
        {
            asteroids[i].has_colided = false;
            transformer.rotateMeshAsset(asteroids[i].mesh, 0.0, -asteroids[i].y_rotation, -asteroids[i].z_rotation); //  Invert sign of last rotate + translate to move back to origin
            transformer.translateMeshAsset(asteroids[i].mesh, -60.0, static_cast<float>(-asteroids[i].y_spawn_offset), static_cast<float>(-asteroids[i].z_spawn_offset));

            asteroids[i].z_rotation = 0.0;
            asteroids[i].y_rotation = 0.0;

            generate_random_numbers();

            transformer.translateMeshAsset(asteroids[i].mesh, 0.0, static_cast<float>(rand_offset_y), static_cast<float>(rand_offset_z));                     //  Perform new rand translation / asteroid spawn and update
            asteroids[i].z_spawn_offset = rand_offset_z;
            asteroids[i].y_spawn_offset = rand_offset_y;
        };

        int collision = 0;
        collision = check_collisions(spaceship, asteroids[i].mesh);

        if((collision == 1) && asteroids[i].has_colided == false)
        {
            asteroids[i].has_colided = true;
            ship_health = ship_health - collision_damage;

            generate_random_numbers();
            asteroids[i].z_rotation = rand_offset_z * 3;
            asteroids[i].y_rotation = rand_offset_y * 3;

            transformer.rotateMeshAsset(asteroids[i].mesh, 0.0, asteroids[i].y_rotation, asteroids[i].z_rotation);

            std::string hp = std::string("Ship health: ").append(std::to_string(ship_health));
            text_manager->loadText(hp, 0, 0, 10, 1.0f, 0.0f, 0.0f, healthText);

            if(ship_health <= 0)
            {
                game_over = true;
            };
        };
    };
};

int main()
{
    init();

    window->open();

    while(window->isOpen)
    {
        if(game_over)
        {
            game_end();
        };

        event_manager.listen();

        camera_manager->loadCamera(camera);
        light_manager->loadLightSource(light);

        draw_assets();
        process_user_inputs();
        move_asteroids();

        int status = globals.getExecutionState();
        
        if(status != LAZARUS_OK)
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