#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <learnopengl/shader_m.h>
#include "Polygon.cpp"
#include <stb_image.h>
#include "PolygonWithTexture.cpp"
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <random>
#include "Camera.cpp"
#include "Model.h"

// بطاقة وحدة = مضلعين (الوجه + الخلفية) مع حركة (Animation)
struct Card {
    std::unique_ptr<PolygonWithTexture> front;
    std::unique_ptr<PolygonWithTexture> back;

    // حالة الحركة
    glm::vec3 startPos{0.0f}, pos{0.0f}, targetPos{0.0f};
    float startAngle = 0.0f, angle = 0.0f, targetAngle = 0.0f; // 0 = أفقي، 90 = واقف
    float t = 1.0f;            // تقدّم الحركة من 0 إلى 1
    float duration = 0.6f;     // مدة الحركة بالثواني
    float arcHeight = 0.0f;    // ارتفاع القوس أثناء الانتقال
    bool animating = false;

    // يطبّق نفس المصفوفة على الوجهين ليتحركوا سوا
    void setTransform(const glm::mat4& m)
    {
        front->transformation(m);
        back->transformation(m);
    }

    // يبني المصفوفة من المكان والزاوية الحاليين
    void applyTransform()
    {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), pos);
        if (angle != 0.0f)
            m = glm::rotate(m, glm::radians(angle), glm::vec3(1.0f, 0.0f, 0.0f));
        setTransform(m);
    }

    // وضع فوري بدون حركة (للبداية)
    void snapTo(const glm::vec3& p, float a)
    {
        startPos = pos = targetPos = p;
        startAngle = angle = targetAngle = a;
        t = 1.0f;
        animating = false;
        applyTransform();
    }

    // بدء حركة من الوضع الحالي إلى هدف جديد
    void moveTo(const glm::vec3& p, float a, float dur, float arc)
    {
        startPos = pos;
        startAngle = angle;
        targetPos = p;
        targetAngle = a;
        duration = dur;
        arcHeight = arc;
        t = 0.0f;
        animating = true;
    }

    // تُستدعى كل frame
    void update(float dt)
    {
        if (!animating)
            return;

        t = std::min(1.0f, t + dt / duration);//زمن كل إطار سابق على الزمن الكلي للتحريك
        const float e = t * t * (3.0f - 2.0f * t);   // smoothstep: بداية ونهاية ناعمة

        pos = glm::mix(startPos, targetPos, e);
        pos.y += std::sin(3.14159265f * t) * arcHeight;   // قوس يرفع البطاقة بنص الطريق
        angle = glm::mix(startAngle, targetAngle, e);

        applyTransform();

        if (t >= 1.0f)
            animating = false;
    }

    void draw(Shader& shader)
    {
        front->draw(shader);
        back->draw(shader);
    }

    void deleteBuffers()
    {
        front->deleteBuffers();
        back->deleteBuffers();
    }
};

const float CARD_HALF_DEPTH = 0.26f;    //   الفكرة انه وقت نوقف البطاقة ما تغوص بقلب الطاولة..نص طول البطاقة (0.52 / 2)
const float TABLE_SURFACE_Y = 1.014f;   // سطح البساط//هون الفكرة اانا نعرف ارتفاع البطاقة لما نوقفها

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

Camera camera(glm::vec3(0.0f, 2.35f, 3.4f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;//عشان ما يصير في قفزات مفاجئة

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// 1 = top view (horizontal cards), 2 = player view (upright cards).
int currentCameraMode = 2;

void processInput(
    GLFWwindow *window,
    std::vector<Card>& cards,
    const std::vector<glm::vec3>& slotPositions,//الخانات يلي رح تنحط فيها البطاقات
    int& nextCardIndex//رقم البطاقة يلي يجب سحبها
);
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn);//تغيير اتجاه النظر
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);//تغيير زاوية النظر..نكبر المشهد او نصغره
void framebuffer_size_callback(GLFWwindow* window, int width, int height);//تغيير الفريم الخاص بالنافذة
void addBoxPolygons(std::vector<Polygon>& list,float cx, float cy, float cz,float sx, float sy, float sz,glm::vec3 color,const std::string& baseName);//رسم مكعب
std::vector<glm::vec3> makeHorizontalQuad(float centerX, float centerY, float centerZ, float width, float depth);//رسم سطح أفقي
std::vector<glm::vec3> makeVerticalQuad(float centerX, float centerY, float centerZ, float width, float height);//رسم شكل رباعي عمودي
GLuint loadTexture(const char* path);
void dealNextCard(//مسؤولة عن سحب البطاقات ووضعها على الطاولة
    std::vector<Card>& cards,
    std::vector<GLuint>& faceTextures,
    const std::vector<glm::vec3>& slotPositions,
    int& nextCardIndex,
    int cameraMode
);

void getCardTarget(//بتحدد المكان والزاوية يلي بدها توصل البطاقة الهن
    int index,
    int nextCardIndex,
    int cameraMode,
    const std::vector<glm::vec3>& slotPositions,
    glm::vec3& outPos,
    float& outAngle
);

void updateCardsForView(//تحديث وضعية البطاقات لما بدنا نغير منظور الكاميرا
    std::vector<Card>& cards,
    const std::vector<glm::vec3>& slotPositions,
    int nextCardIndex,
    int cameraMode,
    bool instant = false//هون منحدد اذا بدنا انميشن او لا
);

// Welcome screen state.
bool inWelcomeScreen = true;
bool isFadingOut = false;//تأثير بصري بخلي الشاشة تختفي تدريجيا
float welcomeBrightness = 1.0f;//يتحكم بدرجة السطوع
float welcomeTimer = 0.0f;

void drawWelcomeScreen(GLFWwindow* window, Shader& shader,
                      PolygonWithTexture& welcomeQuad, float frameDelta)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (!isFadingOut)
    {
        welcomeTimer += frameDelta;
        if (welcomeTimer >= 3.0f ||
            glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS)
        {
            isFadingOut = true;
        }
    }

    float brightness = 1.0f;
    if (isFadingOut)
    {
        welcomeBrightness -= frameDelta * 1.5f;
        if (welcomeBrightness <= 0.0f)
        {
            welcomeBrightness = 0.0f;
            inWelcomeScreen = false;
        }
        brightness = welcomeBrightness;
    }
    else
    {
        brightness = 0.88f + 0.12f * std::sin(
            static_cast<float>(glfwGetTime()) * 2.5f);
    }

    glDisable(GL_DEPTH_TEST);

    const glm::mat4 identity(1.0f);
    shader.setMat4("projection", identity);
    shader.setMat4("view", identity);
    shader.setBool("useTexture", true);
    shader.setBool("isCloak", false);
    shader.setVec3("objectColor", glm::vec3(brightness));

    welcomeQuad.draw(shader);

    glEnable(GL_DEPTH_TEST);
    shader.setVec3("objectColor", glm::vec3(1.0f));
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // اطلب 4 عينات لتنعيم حواف المجسمات (MSAA).
    glfwWindowHint(GLFW_SAMPLES, 4);

    // افتح نافذة اللعبة مكبّرة لتستفيد من مساحة الشاشة المتاحة.
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Anime Card Game", nullptr, nullptr);
    if (!window) {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    camera.SetPose(
        glm::vec3(0.0f, 2.35f, 3.4f),
        -90.0f,
        -15.25f,
        45.0f
    );

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);//هون منحدد اذا مؤشو الماوس رح يظهر او لا

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    // تفعيل تنعيم الحواف إذا كان سياق OpenGL قد أنشأ عينات MSAA.
    glEnable(GL_MULTISAMPLE);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);//هدول توابع الهن علاقة بالشفافية

    // أبعاد الـ Framebuffer الفعلية بالبكسل، وقد تختلف عن أبعاد النافذة المنطقية.
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    Shader shader("../GLProject/shaders/vs/vertex.vs", "../GLProject/shaders/fs/fragment.fs");
    Shader modelShader("../GLProject/shaders/vs/vertex-Model.vs", "../GLProject/shaders/fs/fragment-Model.fs");

    std::vector<Polygon> roomAndTablePolygons;//تخزين مجسمات الديكور
    std::vector<Card> deckCards;//تخزين البطاقات
    std::vector<GLuint> cardFaceTextures;//تخزين رقم التعريف للبطاقات
    const std::vector<std::string> cardImagePaths = {
        "textures/card1.jpg",
        "textures/card2.jpg",
        "textures/card3.jpg",
        "textures/card4.jpg",
        "textures/card5.jpg"
    };

    for (const std::string& path : cardImagePaths)
        cardFaceTextures.push_back(loadTexture(path.c_str()));

    std::mt19937 randomEngine(static_cast<unsigned int>(std::time(nullptr)));//بيخلط البطاقات عشوائيا
    std::shuffle(cardFaceTextures.begin(), cardFaceTextures.end(), randomEngine);

    GLuint cardBackTexture = loadTexture("textures/card_back.jpg");


    const std::vector<glm::vec3> cardSlotPositions = {
        {-0.57f, 1.014f, 0.49f},
        {-0.145f, 1.014f, 0.49f},
        { 0.28f, 1.014f, 0.49f},
        { 0.705f, 1.014f, 0.49f},
        { 1.13f, 1.014f, 0.49f}
    };

    const std::vector<glm::vec3> cardVertices =
        makeHorizontalQuad(0.0f, 0.0f, 0.0f, 0.33f, 0.52f);


    const std::vector<glm::vec2> cardUVs = {//تحديد الزوايا الاربعة للصق الصورة ع البطاقة
        {0.0f, 1.0f},
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        {0.0f, 0.0f}
    };

    // مضلع الخلفية (تحت الوجه بقليل، بترتيب معكوس)
    const std::vector<glm::vec3> cardBackVertices = {
        {-0.165f, -0.003f, -0.26f},
        {-0.165f, -0.003f,  0.26f},
        { 0.165f, -0.003f,  0.26f},
        { 0.165f, -0.003f, -0.26f}
    };
    const std::vector<glm::vec2> cardBackUVs = {
        {1.0f, 1.0f},
        {1.0f, 0.0f},
        {0.0f, 0.0f},
        {0.0f, 1.0f}
    };


    // إنشاء البطاقات (وجه + خلفية) ووضعها فوراً بالمخزون
    for (int i = 0; i < 5; ++i)
    {
        Card card;
        card.front = std::make_unique<PolygonWithTexture>(cardVertices, cardUVs);
        card.front->setTexture(cardFaceTextures[i]);
        card.back = std::make_unique<PolygonWithTexture>(cardBackVertices, cardBackUVs);
        card.back->setTexture(cardBackTexture);

        card.snapTo(glm::vec3(-1.45f, 1.02f + (4 - i) * 0.012f, 0.75f), 0.0f);//بتحط البطاقات بالمخزون بدون انميشن
        deckCards.push_back(std::move(card));
    }
    std::vector<Polygon> decorPolygons;


    // 2. الطاولة
    addBoxPolygons(roomAndTablePolygons, 0.0f, 0.94f, 0.0f, 4.4f, 0.12f, 2.8f, glm::vec3(0.18f, 0.17f, 0.19f), "TableTop");

    addBoxPolygons(roomAndTablePolygons, -2.0f, 0.44f,  1.2f, 0.16f, 0.88f, 0.16f, glm::vec3(0.14f, 0.13f, 0.15f), "TableLeg_FL");
    addBoxPolygons(roomAndTablePolygons,  2.0f, 0.44f,  1.2f, 0.16f, 0.88f, 0.16f, glm::vec3(0.14f, 0.13f, 0.15f), "TableLeg_FR");
    addBoxPolygons(roomAndTablePolygons, -2.0f, 0.44f, -1.2f, 0.16f, 0.88f, 0.16f, glm::vec3(0.14f, 0.13f, 0.15f), "TableLeg_BL");
    addBoxPolygons(roomAndTablePolygons,  2.0f, 0.44f, -1.2f, 0.16f, 0.88f, 0.16f, glm::vec3(0.14f, 0.13f, 0.15f), "TableLeg_BR");


    // 5. الديكور
    // أ) بترينة طويلة بثلاثة رفوف
    glm::vec3 wood(0.60f, 0.40f, 0.22f);
    glm::vec3 woodLight(0.72f, 0.50f, 0.30f);   // الرفوف أفتح
    glm::vec3 woodDark(0.22f, 0.13f, 0.07f);    // الظهر أغمق ليبين العمق
    glm::vec3 trim(0.40f, 0.26f, 0.14f);        // الإطار والقاعدة

    // القاعدة (ارتفاع 0.15) + إطار سفلي بارز
    addBoxPolygons(decorPolygons, 3.2f, 0.075f, -5.35f, 1.40f, 0.15f, 0.66f, trim, "Cabinet_Plinth");
    // الجانبان (من 0.15 إلى 3.05)
    addBoxPolygons(decorPolygons, 2.575f, 1.60f, -5.35f, 0.06f, 3.00f, 0.60f, wood, "Cabinet_SideL");
    addBoxPolygons(decorPolygons, 3.825f, 1.60f, -5.35f, 0.06f, 3.00f, 0.60f, wood, "Cabinet_SideR");
    // الظهر
    addBoxPolygons(decorPolygons, 3.2f, 1.60f, -5.64f, 1.20f, 3.00f, 0.04f, woodDark, "Cabinet_Back");
    // السقف مع كورنيش بارز
    addBoxPolygons(decorPolygons, 3.2f, 3.08f, -5.35f, 1.30f, 0.06f, 0.60f, wood, "Cabinet_Top");
    addBoxPolygons(decorPolygons, 3.2f, 3.14f, -5.35f, 1.42f, 0.06f, 0.68f, trim, "Cabinet_Cornice");

    // الرفوف الثلاثة (فواصل متساوية: 0.15 → 3.05)
    addBoxPolygons(decorPolygons, 3.2f, 0.95f, -5.35f, 1.20f, 0.06f, 0.56f, woodLight, "Cabinet_Shelf1");
    addBoxPolygons(decorPolygons, 3.2f, 1.80f, -5.35f, 1.20f, 0.06f, 0.56f, woodLight, "Cabinet_Shelf2");
    addBoxPolygons(decorPolygons, 3.2f, 2.60f, -5.35f, 1.20f, 0.06f, 0.56f, woodLight, "Cabinet_Shelf3");
    // الرف السفلي (سطحه 0.98): كتب
    addBoxPolygons(decorPolygons, 2.85f, 1.18f, -5.40f, 0.07f, 0.40f, 0.30f, glm::vec3(0.15f, 0.15f, 0.18f), "Book1");
    addBoxPolygons(decorPolygons, 2.93f, 1.16f, -5.40f, 0.07f, 0.36f, 0.30f, glm::vec3(0.85f, 0.85f, 0.85f), "Book2");
    addBoxPolygons(decorPolygons, 3.01f, 1.18f, -5.40f, 0.07f, 0.40f, 0.30f, glm::vec3(0.60f, 0.10f, 0.10f), "Book3");
    addBoxPolygons(decorPolygons, 3.09f, 1.16f, -5.40f, 0.07f, 0.36f, 0.30f, glm::vec3(0.10f, 0.25f, 0.50f), "Book4");
    addBoxPolygons(decorPolygons, 3.17f, 1.18f, -5.40f, 0.07f, 0.40f, 0.30f, glm::vec3(0.20f, 0.45f, 0.30f), "Book5");

    // الرف الأوسط (سطحه 1.83): طاقية لوفي
   
 #pragma region StrawHat
// موقع القبعة على الرف
const float hatX = 3.2f;
const float hatZ = -5.38f;

const int segments = 64;
const float PI = 3.14159265f;

// ألوان القش والشريط
glm::vec3 strawColor(0.76f, 0.62f, 0.39f);
glm::vec3 strawSideColor(0.61f, 0.46f, 0.26f);
glm::vec3 redBand(0.62f, 0.025f, 0.02f);

// حساب نقطة على دائرة
auto hatPoint = [&](float radius, float y, float angle)
{
    return glm::vec3(
        hatX + radius * std::cos(angle),
        y,
        hatZ + radius * std::sin(angle)
    );
};

// =====================================
// 1. الحافة: انحناء تدريجي بدلاً من سطح مسطح
// =====================================

// أنصاف أقطار الحافة من الداخل للخارج
const float brimRadii[] = {
    0.218f,
    0.265f,
    0.33f,
    0.39f
};

// ارتفاع كل حلقة
const float brimHeights[] = {
    1.85f,
    1.862f,
    1.854f,
    1.842f
};

// إنشاء سطح الحافة على شكل حلقات متصلة
for (int ring = 0; ring < 3; ring++)
{
    for (int i = 0; i < segments; i++)
    {
        float a0 = 2.0f * PI * i / segments;
        float a1 = 2.0f * PI * (i + 1) / segments;

        decorPolygons.emplace_back(
            std::vector<glm::vec3>{
                hatPoint(brimRadii[ring], brimHeights[ring], a0),
                hatPoint(brimRadii[ring], brimHeights[ring], a1),
                hatPoint(brimRadii[ring + 1], brimHeights[ring + 1], a1),
                hatPoint(brimRadii[ring + 1], brimHeights[ring + 1], a0)
            },
            strawColor * (1.0f - ring * 0.012f),
            "Hat_Brim_" + std::to_string(ring) +
            "_" + std::to_string(i)
        );
    }
}

// سماكة الحافة الخارجية
const float brimBottom = 1.835f;

for (int i = 0; i < segments; i++)
{
    float a0 = 2.0f * PI * i / segments;
    float a1 = 2.0f * PI * (i + 1) / segments;

    decorPolygons.emplace_back(
        std::vector<glm::vec3>{
            hatPoint(0.39f, brimBottom, a0),
            hatPoint(0.39f, 1.842f, a0),
            hatPoint(0.39f, 1.842f, a1),
            hatPoint(0.39f, brimBottom, a1)
        },
        strawSideColor,
        "Hat_BrimSide_" + std::to_string(i)
    );
}

// =====================================
// 2. التاج: قبة أقصر وأكثر استدارة
// =====================================

const float crownY[] = {
    1.85f,
    1.88f,
    1.915f,
    1.95f,
    1.98f,
    2.005f,
    2.02f
};

const float crownR[] = {
    0.218f,
    0.219f,
    0.204f,
    0.177f,
    0.14f,
    0.09f,
    0.03f
};

const int crownLevels = 7;

for (int level = 0; level < crownLevels - 1; level++)
{
    for (int i = 0; i < segments; i++)
    {
        float a0 = 2.0f * PI * i / segments;
        float a1 = 2.0f * PI * (i + 1) / segments;

        decorPolygons.emplace_back(
            std::vector<glm::vec3>{
                hatPoint(crownR[level], crownY[level], a0),
                hatPoint(crownR[level + 1], crownY[level + 1], a0),
                hatPoint(crownR[level + 1], crownY[level + 1], a1),
                hatPoint(crownR[level], crownY[level], a1)
            },
            strawColor * (1.0f - level * 0.012f),
            "Hat_Crown_" + std::to_string(level) +
            "_" + std::to_string(i)
        );
    }
}

// إغلاق أعلى القبة
for (int i = 0; i < segments; i++)
{
    float a0 = 2.0f * PI * i / segments;
    float a1 = 2.0f * PI * (i + 1) / segments;

    decorPolygons.emplace_back(
        std::vector<glm::vec3>{
            glm::vec3(hatX, 2.02f, hatZ),
            hatPoint(0.03f, 2.02f, a1),
            hatPoint(0.03f, 2.02f, a0)
        },
        strawColor * 0.94f,
        "Hat_CrownTop_" + std::to_string(i)
    );
}

// =====================================
// 3. الشريط الأحمر: أنحف ومتناسق
// =====================================

const float bandBottomY = 1.857f;
const float bandTopY = 1.888f;

const float bandBottomRadius = 0.226f;
const float bandTopRadius = 0.221f;

for (int i = 0; i < segments; i++)
{
    float a0 = 2.0f * PI * i / segments;
    float a1 = 2.0f * PI * (i + 1) / segments;

    decorPolygons.emplace_back(
        std::vector<glm::vec3>{
            hatPoint(bandBottomRadius, bandBottomY, a0),
            hatPoint(bandTopRadius, bandTopY, a0),
            hatPoint(bandTopRadius, bandTopY, a1),
            hatPoint(bandBottomRadius, bandBottomY, a1)
        },
        redBand,
        "Hat_RedBand_" + std::to_string(i)
    );
}
#pragma endregion
    // الرف العلوي (سطحه 2.63): مجسمات صغيرة
    addBoxPolygons(decorPolygons, 2.95f, 2.77f, -5.38f, 0.22f, 0.28f, 0.22f, glm::vec3(0.80f, 0.20f, 0.15f), "Figure1");
    addBoxPolygons(decorPolygons, 3.25f, 2.80f, -5.38f, 0.22f, 0.34f, 0.22f, glm::vec3(0.95f, 0.65f, 0.10f), "Figure2");
    addBoxPolygons(decorPolygons, 3.55f, 2.76f, -5.38f, 0.22f, 0.26f, 0.22f, glm::vec3(0.15f, 0.35f, 0.70f), "Figure3");

    // ب) البيانو (مكعب مرجعي)
    Model piano("models/piano.obj");

    if (!piano.IsLoaded())
    {
        std::cout << "Piano loading failed!\n";
    }
    glm::mat4 pianoTransform = piano.GetFitTransform(
        glm::vec3(-4.45f, 1.0f, -4.20f),
        glm::vec3(2.6f, 2.0f, 3.0f),
        90.0f
    );
    // ج) علاقة العباية (بدون عباءة)
    addBoxPolygons(decorPolygons, 4.9f, 0.03f, -4.8f, 0.80f, 0.06f, 0.80f, glm::vec3(0.20f, 0.20f, 0.22f), "CoatRack_Base");
    addBoxPolygons(decorPolygons, 4.9f, 1.53f, -4.8f, 0.08f, 3.00f, 0.08f, glm::vec3(0.25f, 0.25f, 0.28f), "CoatRack_Pole");
    addBoxPolygons(decorPolygons, 4.9f, 2.95f, -4.8f, 1.00f, 0.07f, 0.07f, glm::vec3(0.25f, 0.25f, 0.28f), "CoatRack_TopBar");

    // --- عباءة إيتاتشي معلّقة على العلّاقة ---
    GLuint cloakTex = loadTexture("../GLProject/textures/itatchi.jpg");

    std::vector<glm::vec2> cloakUVs = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };

    PolygonWithTexture itachiCloak(
        {
            {4.10f, 0.85f, -4.74f}, // سفلي يسار
            {5.70f, 0.85f, -4.74f}, // سفلي يمين
            {5.70f, 3.10f, -4.74f}, // علوي يمين (فوق البار بشوي ليركب الخطّاف)
            {4.10f, 3.10f, -4.74f}  // علوي يسار
        },
        cloakUVs
    );
    itachiCloak.setTexture(cloakTex);

    GLuint playmatTex = loadTexture("../GLProject/textures/playmat.jpg");

    std::vector<glm::vec3> matVerts = {
        {-1.20f, 1.003f,  1.15f},
        { 1.80f, 1.003f,  1.15f},
        { 1.80f, 1.003f, -1.15f},
        {-1.20f, 1.003f, -1.15f}
    };
    std::vector<glm::vec2> matUVs = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };

    PolygonWithTexture playmat(matVerts, matUVs);
    playmat.setTexture(playmatTex);

    GLuint wallTex = loadTexture("../GLProject/textures/wallpaper.jpg");

    std::vector<glm::vec2> wallUVs = {
        {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
    };
    GLuint ceilingTex = loadTexture("../GLProject/textures/wallpaper.jpg");

    PolygonWithTexture ceiling(
        {{-6.0f, 4.5f, -6.0f}, {6.0f, 4.5f, -6.0f}, {6.0f, 4.5f, 6.0f}, {-6.0f, 4.5f, 6.0f}},
        wallUVs
    );
    ceiling.setTexture(ceilingTex); 

    PolygonWithTexture backWall(
        {{-6.0f, 0.0f, -6.0f}, {6.0f, 0.0f, -6.0f}, {6.0f, 4.5f, -6.0f}, {-6.0f, 4.5f, -6.0f}},
        wallUVs
    );
    backWall.setTexture(wallTex);

    PolygonWithTexture leftWall(
        {{-6.0f, 0.0f, 6.0f}, {-6.0f, 0.0f, -6.0f}, {-6.0f, 4.5f, -6.0f}, {-6.0f, 4.5f, 6.0f}},
        wallUVs
    );
    leftWall.setTexture(wallTex);
    PolygonWithTexture rightWall(
        {{6.0f, 0.0f, -6.0f}, {6.0f, 0.0f, 6.0f}, {6.0f, 4.5f, 6.0f}, {6.0f, 4.5f, -6.0f}},
        wallUVs
    );
    rightWall.setTexture(wallTex);

    GLuint floorTex = loadTexture("../GLProject/textures/floor.jpg");

    PolygonWithTexture floor(
        {{-6.0f, 0.0f,  6.0f}, {6.0f, 0.0f,  6.0f}, {6.0f, 0.0f, -6.0f}, {-6.0f, 0.0f, -6.0f}},
        wallUVs
    );
    floor.setTexture(floorTex);

    // --- البوستر الممزق (بشكل عرضي) على الحائط الخلفي ---
    GLuint posterTex = loadTexture("../GLProject/textures/poster.jpg");

    // 5 نقاط: بوستر عرضي ومقطوعة زاويته السفلية اليمين (ممزق)
    std::vector<glm::vec3> tornPosterVerts = {
        {-1.8f, 1.35f, -5.95f}, // 1. سفلي يسار
        { 0.5f, 1.35f, -5.95f}, // 2. بداية التمزق من تحت
        { 1.0f, 2.00f, -5.95f}, // 3. نهاية التمزق من اليمين
        { 1.0f, 3.15f, -5.95f}, // 4. علوي يمين
        {-1.8f, 3.15f, -5.95f}  // 5. علوي يسار
    };
    std::vector<glm::vec2> tornPosterUVs = {
        {0.00f, 0.00f}, // 1
        {0.82f, 0.00f}, // 2
        {1.00f, 0.36f}, // 3
        {1.00f, 1.00f}, // 4
        {0.00f, 1.00f}  // 5
    };

    PolygonWithTexture poster(tornPosterVerts, tornPosterUVs);
    poster.setTexture(posterTex);

    GLuint welcomeTex = loadTexture("../GLProject/textures/welcome.jpg");

    const std::vector<glm::vec3> welcomeVerts = {
        {-1.0f, -1.0f, 0.0f},
        { 1.0f, -1.0f, 0.0f},
        { 1.0f,  1.0f, 0.0f},
        {-1.0f,  1.0f, 0.0f}
    };
    const std::vector<glm::vec2> welcomeUVs = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {1.0f, 1.0f},
        {0.0f, 1.0f}
    };

    PolygonWithTexture welcomeQuad(welcomeVerts, welcomeUVs);
    welcomeQuad.setTexture(welcomeTex);

    int nextCardIndex = 0;
    // وضع البداية فوري بدون حركة
    updateCardsForView(deckCards, cardSlotPositions, nextCardIndex, currentCameraMode, true);
    bool gWasPressed = false;

    lastFrame = static_cast<float>(glfwGetTime());
    welcomeTimer = 0.0f;
    welcomeBrightness = 1.0f;

    bool welcomeCursorReleased = true;

    while (!glfwWindowShouldClose(window))
    {
        const float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        if (deltaTime > 0.1f)
            deltaTime = 0.1f;

        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth == 0 || framebufferHeight == 0)
        {
            glfwPollEvents();
            continue;
        }

        shader.use();

        if (inWelcomeScreen)
        {
            drawWelcomeScreen(window, shader, welcomeQuad, deltaTime);
            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }

        if (welcomeCursorReleased)
        {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
            welcomeCursorReleased = false;
        }

        processInput(window, deckCards, cardSlotPositions, nextCardIndex);

        const bool gIsPressed = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
        if (gIsPressed && !gWasPressed)
        {
            dealNextCard(
                deckCards, cardFaceTextures, cardSlotPositions,
                nextCardIndex, currentCameraMode
            );
        }
        gWasPressed = gIsPressed;

        const glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
            0.1f,
            100.0f
        );
        const glm::mat4 view = camera.GetViewMatrix();

        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        shader.setBool("useTexture", false);
        for (auto& polygon : roomAndTablePolygons)
            polygon.draw(shader);
        for (auto& decoration : decorPolygons)
            decoration.draw(shader);
       shader.setBool("useTexture", true);
        shader.setBool("isCloak", false);
        playmat.draw(shader);

        // تحديث حركة البطاقات ثم رسمها
        for (auto& card : deckCards)
            card.update(deltaTime);

        for (auto& card : deckCards)
            card.draw(shader);

        backWall.draw(shader);
        leftWall.draw(shader);
        rightWall.draw(shader);
        ceiling.draw(shader);
        floor.draw(shader);
        poster.draw(shader);

        modelShader.use();
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);
        modelShader.setVec3("lightPos", glm::vec3(-3.8f, 4.0f, -3.0f));
        modelShader.setVec3("viewPos", camera.Position);
        modelShader.setVec3("objectColor", glm::vec3(1.0f));
        modelShader.setMat4("model", pianoTransform);
        piano.Draw(modelShader, pianoTransform);

        shader.use();
        shader.setBool("useTexture", true);
        shader.setBool("isCloak", true);
        itachiCloak.draw(shader);
        shader.setBool("isCloak", false);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    for (auto& p : roomAndTablePolygons) p.deleteBuffers();
    for (auto& card : deckCards)
        card.deleteBuffers();
    for (auto& d : decorPolygons)        d.deleteBuffers();
    playmat.deleteBuffers();
    backWall.deleteBuffers();
    leftWall.deleteBuffers();
    rightWall.deleteBuffers();
    ceiling.deleteBuffers();
    floor.deleteBuffers();
    poster.deleteBuffers();
    itachiCloak.deleteBuffers();
    welcomeQuad.deleteBuffers();

    glDeleteTextures(1, &cardBackTexture);
    for (GLuint textureID : cardFaceTextures)
        glDeleteTextures(1, &textureID);

    glDeleteTextures(1, &welcomeTex);
    glDeleteTextures(1, &cloakTex);
    glDeleteTextures(1, &playmatTex);
    glDeleteTextures(1, &wallTex);
    glDeleteTextures(1, &ceilingTex);
    glDeleteTextures(1, &floorTex);
    glDeleteTextures(1, &posterTex);

    glfwTerminate();
    return 0;
}

// يحسب مكان وزاوية البطاقة الهدف حسب حالتها (مخزون/مسحوبة) ووضع الكاميرا
void getCardTarget(
    int index,
    int nextCardIndex,
    int cameraMode,
    const std::vector<glm::vec3>& slotPositions,
    glm::vec3& outPos,
    float& outAngle
)
{
    if (index < nextCardIndex)
    {
        // بطاقة مسحوبة: في منطقة اللعب
        outPos = slotPositions[index];
        outAngle = 0.0f;
        if (cameraMode == 2)
        {
            outAngle = 90.0f;
            outPos.y = TABLE_SURFACE_Y + CARD_HALF_DEPTH;
            outPos.z += CARD_HALF_DEPTH;
        }
    }
    else
    {
        // بطاقة بالمخزون: مكدسة وأفقية دائماً
        outPos = glm::vec3(-1.45f, 1.02f + (4 - index) * 0.012f, 0.75f);
        outAngle = 0.0f;
    }
}

void dealNextCard(
    std::vector<Card>& cards,
    std::vector<GLuint>& faceTextures,
    const std::vector<glm::vec3>& slotPositions,
    int& nextCardIndex,
    int cameraMode
)
{
    if (nextCardIndex >= static_cast<int>(cards.size()) ||
        nextCardIndex >= static_cast<int>(faceTextures.size()) ||
        nextCardIndex >= static_cast<int>(slotPositions.size()))
    {
        std::cout << "No cards left in the deck.\n";
        return;
    }

    cards[nextCardIndex].front->setTexture(faceTextures[nextCardIndex]);

    glm::vec3 targetPos;
    float targetAngle;
    getCardTarget(nextCardIndex, nextCardIndex + 1, cameraMode,
                  slotPositions, targetPos, targetAngle);

    // مدة 0.8 ثانية، وقوس بارتفاع 0.35
    cards[nextCardIndex].moveTo(targetPos, targetAngle, 0.8f, 0.35f);

    std::cout << "Dealt card " << (nextCardIndex + 1) << "\n";
    ++nextCardIndex;
}

void updateCardsForView(
    std::vector<Card>& cards,
    const std::vector<glm::vec3>& slotPositions,
    int nextCardIndex,
    int cameraMode,
    bool instant
)
{
    for (std::size_t i = 0; i < cards.size() && i < slotPositions.size(); ++i)
    {
        glm::vec3 targetPos;
        float targetAngle;
        getCardTarget(static_cast<int>(i), nextCardIndex, cameraMode,
                      slotPositions, targetPos, targetAngle);

        if (instant)
            cards[i].snapTo(targetPos, targetAngle);
        else
            cards[i].moveTo(targetPos, targetAngle, 0.6f, 0.0f); // بدون قوس: وقوف/تسطيح
    }
}

void processInput(
    GLFWwindow *window,
    std::vector<Card>& cards,
    const std::vector<glm::vec3>& slotPositions,
    int& nextCardIndex
)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    static bool key1WasPressed = false;
    static bool key2WasPressed = false;

    const bool key1IsPressed = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    const bool key2IsPressed = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;

    if (key1IsPressed && !key1WasPressed)
    {
        camera.SetPose(
            glm::vec3(0.0f, 4.35f, 0.0f),
            -90.0f,
            -89.0f,
            45.0f
        );
        currentCameraMode = 1;
        updateCardsForView(cards, slotPositions, nextCardIndex, currentCameraMode);
    }

    if (key2IsPressed && !key2WasPressed)
    {
        camera.SetPose(
            glm::vec3(0.0f, 2.35f, 3.4f),
            -90.0f,
            -15.25f,
            45.0f
        );
        currentCameraMode = 2;
        updateCardsForView(cards, slotPositions, nextCardIndex, currentCameraMode);
    }

    key1WasPressed = key1IsPressed;
    key2WasPressed = key2IsPressed;
}

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    (void)window; // غير مستخدم داخل الدالة
    (void)xoffset; // نستخدم مقدار الحركة العمودية فقط
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

// مضلع رباعي أفقي في مستوى XZ عند ارتفاع y
std::vector<glm::vec3> makeHorizontalQuad(float centerX, float y, float centerZ, float width, float depth) {
    float hw = width * 0.5f;
    float hd = depth * 0.5f;
    return {
        glm::vec3(centerX - hw, y, centerZ - hd),
        glm::vec3(centerX + hw, y, centerZ - hd),
        glm::vec3(centerX + hw, y, centerZ + hd),
        glm::vec3(centerX - hw, y, centerZ + hd)
    };
}

// مضلع رباعي عمودي في مستوى XY عند عمق z
std::vector<glm::vec3> makeVerticalQuadXY(float centerX, float centerY, float z, float width, float height) {
    float hw = width * 0.5f;
    float hh = height * 0.5f;
    return {
        glm::vec3(centerX - hw, centerY - hh, z),
        glm::vec3(centerX + hw, centerY - hh, z),
        glm::vec3(centerX + hw, centerY + hh, z),
        glm::vec3(centerX - hw, centerY + hh, z)
    };
}
// متوازي مستطيلات من 6 أوجه رباعية
void addBoxPolygons(std::vector<Polygon>& list,float cx, float cy, float cz,float sx, float sy, float sz,glm::vec3 color,const std::string& baseName) {
    float hx = sx * 0.5f;
    float hy = sy * 0.5f;
    float hz = sz * 0.5f;
    // الأمامي (+Z)
    list.emplace_back(std::vector<glm::vec3>{
        {cx - hx, cy - hy, cz + hz},
        {cx + hx, cy - hy, cz + hz},
        {cx + hx, cy + hy, cz + hz},
        {cx - hx, cy + hy, cz + hz}
    }, color, baseName + "_Front");

    // الخلفي (-Z)
    list.emplace_back(std::vector<glm::vec3>{
        {cx + hx, cy - hy, cz - hz},
        {cx - hx, cy - hy, cz - hz},
        {cx - hx, cy + hy, cz - hz},
        {cx + hx, cy + hy, cz - hz}
    }, color * 0.9f, baseName + "_Back");

    // العلوي (+Y)
    list.emplace_back(std::vector<glm::vec3>{
        {cx - hx, cy + hy, cz + hz},
        {cx + hx, cy + hy, cz + hz},
        {cx + hx, cy + hy, cz - hz},
        {cx - hx, cy + hy, cz - hz}
    }, glm::min(color * 1.05f, glm::vec3(1.0f)), baseName + "_Top");

    // السفلي (-Y)
    list.emplace_back(std::vector<glm::vec3>{
        {cx - hx, cy - hy, cz - hz},
        {cx + hx, cy - hy, cz - hz},
        {cx + hx, cy - hy, cz + hz},
        {cx - hx, cy - hy, cz + hz}
    }, color * 0.75f, baseName + "_Bottom");

    // الأيمن (+X)
    list.emplace_back(std::vector<glm::vec3>{
        {cx + hx, cy - hy, cz + hz},
        {cx + hx, cy - hy, cz - hz},
        {cx + hx, cy + hy, cz - hz},
        {cx + hx, cy + hy, cz + hz}
    }, color * 0.95f, baseName + "_Right");

    // الأيسر (-X)
    list.emplace_back(std::vector<glm::vec3>{
        {cx - hx, cy - hy, cz - hz},
        {cx - hx, cy - hy, cz + hz},
        {cx - hx, cy + hy, cz + hz},
        {cx - hx, cy + hy, cz - hz}
    }, color * 0.85f, baseName + "_Left");
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

GLuint loadTexture(const char* path) {
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    // الرقم 4 بالأخير بيجبر أي صورة (حتى لو أبيض وأسود أو WebP/PNG غريب) تتحول لـ RGBA بدون كراش
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 4);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "Failed to load texture: " << path << "\n";
    }
    stbi_image_free(data);
    return textureID;
}