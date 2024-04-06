#version 330 core 

//point light

uniform sampler2D tex0;

uniform vec3 lightPos;
uniform vec3 lightColor;

uniform float ambientStr;
uniform vec3 ambientColor;
uniform vec3 cameraPos;
uniform float specStr;
uniform float specPhong;


uniform vec3 dirLight;//direction of the light to the model

uniform float constant;//components for point light attenuation computation
uniform float linear;
uniform float quadratic;

uniform float dirIntensity;
uniform float pointIntensity; //values of brightness 

in vec2 texCoord;

in vec3 normCoord;
in vec3 fragPos;

out vec4 FragColor;


void main()
{

  vec3 normal = normalize(normCoord);

  float distance = length(lightPos - fragPos);//gets the distance from the light source to the object 
  float attenuation = pointIntensity/(constant+linear*distance+quadratic*(distance*distance));//and uses it to compute attenuation alongside the variables:
  //constant, linear and quadratic.
  
  vec3 lightDir = normalize(lightPos - fragPos); //instead of this, we get the direction of the light directly
  vec3 directionLight = normalize(-dirLight);//and then reverse it so that it is instead a vector towards the light source, as was previously done.

  vec3 ambientCol = lightColor * ambientStr;//ambient lighting

  vec3 viewDir = normalize(cameraPos - fragPos);//specular lighting
  vec3 reflectDir = reflect(-lightDir, normal); //this
  float spec =  pow(max(dot(reflectDir, viewDir),0.1f),specPhong);
  vec3 specColor = spec * specStr * lightColor;

  float diff = max(dot(normal, lightDir),0.0);//diffuse lighting code
  vec3 diffuse = diff * lightColor;

  vec3 reflectDirD = reflect(-directionLight, normal); //this
  float specD =  pow(max(dot(reflectDirD, viewDir),0.1f),specPhong);
  vec3 specColorD = specD * specStr * lightColor;

  float diffD = max(dot(normal, directionLight),0.0);//diffuse lighting code
  vec3 diffuseD = diffD * lightColor;

  vec3 direction = dirIntensity * vec3(specColorD + diffuseD + ambientCol);
  vec3 point = pointIntensity * vec3(specColor + diffuse + ambientCol);


  //now multiplies the attenuation and brightness to the lighting 
  FragColor =  vec4((direction + point),1.0) * texture(tex0,texCoord);
}