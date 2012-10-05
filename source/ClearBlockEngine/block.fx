matrix matWorld : WORLD;
matrix matView : VIEW;
matrix matProj : PROJ;

float3 g_vEyeDir : EYEDIR;
float  g_lightRadius : LIGHTRADIUS;
float4 g_lightPos : LIGHTPOS;

Texture2D g_textureAtlas[8] : TEXTUREATLAS;
float4 g_blockNormals[4] : BLOCKNORMALS;

struct BlockType
{
	float2 texCoord;
	float relTexSize;
	int atlasIndex;
	float4 color;
};

BlockType g_blockTypes[1024] : BLOCKTYPES;

SamplerState sam
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Mirror;
    AddressV = Mirror;
};

struct PSINPUT
{
	float4 position : SV_POSITION;
	float4 worldSpace : WORLDSPACE;
	float3 normal : NORMAL0;
	float2 texCoord : TEXCOORD0;
	int typeIndex : TYPEINDEX;
};

//////////////////////////////////////////////////////////////////
//                        VertexShader                          //
//////////////////////////////////////////////////////////////////
PSINPUT simpleVertexShader(	float4 position : POSITION0, 
								float4 normal : NORMAL0, 
								float2 texCoord : TEXCOORD0, 
								int typeIndex : TYPEINDEX)
{
	PSINPUT output = (PSINPUT)0;
	output.worldSpace = mul(position, matWorld);
	output.position = mul(output.worldSpace, matView);
	output.position = mul(output.position, matProj);
	output.normal = (float3)normalize((mul(normal, matWorld)));
	output.texCoord = texCoord;
	output.typeIndex = typeIndex;

	return output;
}

//////////////////////////////////////////////////////////////////
//                        PixelShader                           //
//////////////////////////////////////////////////////////////////
float4 simplePixelShader(PSINPUT input) : SV_Target
{
	while(input.texCoord.x > g_blockTypes[input.typeIndex].texCoord.x + g_blockTypes[input.typeIndex].relTexSize)
		input.texCoord.x -= g_blockTypes[input.typeIndex].relTexSize;

	while(input.texCoord.y > g_blockTypes[input.typeIndex].texCoord.y + g_blockTypes[input.typeIndex].relTexSize)
		input.texCoord.y -= g_blockTypes[input.typeIndex].relTexSize;

	float4 vLightCol = float4(1.0f, 1.0f, 1.0f, 1.0f);
	float4 vLightPos = g_lightPos;
	float  vLightRadius = g_lightRadius;

	float3 vLightDir = -input.worldSpace.xyz + vLightPos.xyz;
	float strength = saturate( 1.0f - length(vLightDir) / vLightRadius );
	strength = strength * strength;

	float specularPower = 5.0f;

	float4 texColor = g_textureAtlas[0].Sample(sam, input.texCoord);
	switch (g_blockTypes[input.typeIndex].atlasIndex)
	{
		case 1: g_textureAtlas[1].Sample(sam, input.texCoord); break;
		case 2: g_textureAtlas[2].Sample(sam, input.texCoord); break;
		case 3: g_textureAtlas[3].Sample(sam, input.texCoord); break;
		case 4: g_textureAtlas[4].Sample(sam, input.texCoord); break;
		case 5: g_textureAtlas[5].Sample(sam, input.texCoord); break;
		case 6: g_textureAtlas[6].Sample(sam, input.texCoord); break;
		case 7: g_textureAtlas[7].Sample(sam, input.texCoord); break;
	}

	float4 lambert = saturate(dot(-normalize(vLightDir), input.normal));
	float4 diffuse = lambert * g_blockTypes[input.typeIndex].color * texColor;
	float4 ambient = g_blockTypes[input.typeIndex].color * texColor * 0.05f;
	
   float3 H = -normalize(g_vEyeDir.xyz) + normalize(vLightDir.xyz);
   float3 halfAngle = normalize( H );
   float _specular = pow( max(0, dot( halfAngle, input.normal)), specularPower );          
   float4 specular = (float4)_specular * vLightCol;

   float thetha = ( dot( input.normal, float3(0.0f, 1.0f, 0.0f)) + 1.0f) / 2.0f;
   float4 hemi = lerp( float4(0.0f, 1.0f, 0.0f, 1.0f), vLightCol, thetha);

	float4 distanceDependend = (diffuse /* + specular*/) * strength;

	float4 finalColor = saturate(distanceDependend + ambient);
	return finalColor;

   // return float4(input.normal.x / 2.0f + 0.5f, input.normal.y / 2.0f + 0.5f, input.normal.z / 2.0f + 0.5f, 1.0f);
}


BlendState NoBlend
{ 
    BlendEnable[0] = False;
};

RasterizerState wireframe
{
	FILLMODE = WIREFRAME;
	CULLMODE = NONE;
};

RasterizerState solid
{
	FILLMODE = SOLID;
	CULLMODE = BACK;
};

//////////////////////////////////////////////////////////////////
//                        Technique                           //
//////////////////////////////////////////////////////////////////
technique11 main
{
	pass 
	{
		SetBlendState( NoBlend, float4( 0.0f, 0.0f, 0.0f, 0.0f ), 0xFFFFFFFF );
		SetVertexShader( CompileShader( vs_4_0, simpleVertexShader() ) );
		SetPixelShader( CompileShader( ps_4_0, simplePixelShader() ) );
	}
}