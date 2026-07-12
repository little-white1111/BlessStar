shader_type spatial;
render_mode unshaded, diffuse_toon;

// 参数
uniform float toon_steps : hint_range(1, 10) = 2.0;
uniform float outline_width : hint_range(0, 50) = 1.0;
uniform float main_light_angle : hint_range(0, 360) = 45.0;
uniform float main_light_intensity : hint_range(0, 5) = 1.0;
uniform float fill_light_intensity : hint_range(0, 5) = 0.3;
uniform float rim_light_intensity : hint_range(0, 5) = 0.0;

// ===== Expression Control Uniforms (driven by ExpressionController) =====
uniform float expr_blush : hint_range(0.0, 1.0) = 0.0;
uniform float expr_eye_openness : hint_range(0.0, 1.0) = 1.0;
uniform float expr_mouth_blend : hint_range(0.0, 1.0) = 0.0;
uniform float expr_brow_angle : hint_range(-1.0, 1.0) = 0.0;
uniform vec4 blush_color : source_color = vec4(1.0, 0.35, 0.35, 1.0);

// 实现两阶阴影 Toon + 轮廓描边 + Rim Light + 表情混合
void vertex() {
    // 法线外扩用于轮廓描边
    VERTEX += NORMAL * outline_width * 0.001;
}

void fragment() {
    // 简化光源方向
    vec3 light_dir = vec3(0.0, 0.0, 1.0);
    float NdotL = dot(NORMAL, light_dir);
    
    // Toon 阶数量化
    float steps = toon_steps;
    float toon = floor(NdotL * steps) / steps;
    toon = mix(toon, 1.0, fill_light_intensity * 0.3);
    
    // Rim Light
    float rim = 1.0 - max(0.0, dot(NORMAL, VIEW));
    rim = pow(rim, 3.0) * rim_light_intensity;

    // 采样 albedo 纹理
    vec3 albedo_tex = texture(ALBEDO_TEXTURE, UV).rgb;
    
    // ===== Expression Blending (GPU smooth interpolation) =====
    // blush overlay — mix face color with pink based on blush intensity
    vec3 expression_color = albedo_tex;
    expression_color = mix(expression_color, blush_color.rgb, expr_blush * 0.6);
    
    // eye openness — reduce eye area albedo contribution when eyes close
    // (requires specific UV mapping to eye region — simplified)
    float eye_factor = mix(0.3, 1.0, expr_eye_openness);
    
    // brow angle — subtle tint shift for brow region
    float brow_tint = expr_brow_angle * 0.15;
    expression_color += brow_tint * vec3(0.1, 0.0, 0.05);
    
    // mouth blend — tint mouth area (requires UV mapping — simplified)
    float mouth_tint = expr_mouth_blend * 0.2;
    expression_color = mix(expression_color, vec3(0.6, 0.2, 0.2), mouth_tint);
    
    // apply eye factor to expression color
    expression_color *= eye_factor;
    
    // 输出最终颜色
    ALBEDO = expression_color * (toon + rim);
    ALPHA = texture(ALBEDO_TEXTURE, UV).a;
}
