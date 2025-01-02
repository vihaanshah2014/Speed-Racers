uniform sampler2D texture;
uniform vec4 color;

void main() {
    vec4 texColor = texture2D(texture, gl_TexCoord[0].xy);
    gl_FragColor = texColor * color;
}