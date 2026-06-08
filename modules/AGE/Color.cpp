#include <AGE/Color.h>

using namespace age;

glm::vec4 Color::HSVAToRGBA(const glm::vec4& hsva){
    float H = hsva.x;
    float S = hsva.y;
    float V = hsva.z;
    float A = hsva.w;

    float C = V * S;
    float X = C * (1 - std::fabs(fmod(H / 60.0f, 2) - 1));
    float m = V - C;

    float r = 0, g = 0, b = 0;

    if (H >= 0 && H < 60) { r = C; g = X; b = 0; }
    else if (H < 120)     { r = X; g = C; b = 0; }
    else if (H < 180)     { r = 0; g = C; b = X; }
    else if (H < 240)     { r = 0; g = X; b = C; }
    else if (H < 300)     { r = X; g = 0; b = C; }
    else if (H <= 360)    { r = C; g = 0; b = X; }

    return glm::vec4(r + m, g + m, b + m, A);
}
glm::vec4 Color::RGBAToHSVA(const glm::vec4& rgba) {
    float R = rgba.r, G = rgba.g, B = rgba.b;
    float maxC = std::max({ R, G, B });
    float minC = std::min({ R, G, B });
    float delta = maxC - minC;

    float H = 0.0f;
    if (delta != 0.0f) {
        if (maxC == R) {
            H = 60.0f * (fmodf((G - B) / delta, 6.0f));
        } else if (maxC == G) {
            H = 60.0f * ((B - R) / delta + 2.0f);
        } else if (maxC == B) {
            H = 60.0f * ((R - G) / delta + 4.0f);
        }
    }

    if (H < 0.0f) H += 360.0f;

    float S = (maxC == 0.0f) ? 0.0f : delta / maxC;
    float V = maxC;

    return glm::vec4(H, S, V, rgba.a); // A 原样保留
}