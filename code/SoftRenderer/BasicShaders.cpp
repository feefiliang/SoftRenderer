#include "stdafx.h"
#include "BasicShaders.h"
#include "SrFragmentBuffer.h"
#include "SrRasterizer.h"

SrFlatShader g_FlatShadingShader;
SrPhongShader g_PhongShadingShader;
SrGourandShader g_GourandShadingShader;
SrPhongWithNormalShader g_PhongShadingWithNormalShader;

// local data format
SR_ALIGN struct SrPhongShading_Vert2Frag
{
	float4 worldpos_tx;		// channel1:xyz
	float4 normal_ty;		// channel1:w channel2:xy
	float4 pos;				// channel0
	float4 tangent;			// channel2:zw channel3:x
	
};

SR_ALIGN struct SrGourandShading_Vert2Frag
{
	float4 pos;					// channel0
	

	float4 diffuseAcc_tx;			// channel1:w channel2:xy
	float4 specularAcc_ty;			// channel1:w channel2:xy

	float4 texcoord;			// channel4:xy
};

SR_ALIGN struct SrFlatShading_Vert2Frag
{
	float4 pos;					// channel0
	

	float4 diffuseAcc_tx;			// channel1:w channel2:xy
	float4 specularAcc_ty;			// channel1:w channel2:xy

	float4 texcoord;			// channel4:xy
};

void SrFlatShader::ProcessPatch( void* vOut, void* vOut1, void* vOut2, const void* vInRef0, const void* vInRef1, const void* vInRef2, const SrShaderContext* context ) const
{
	SrFlatShading_Vert2Frag* inTHREE[3] = {(SrFlatShading_Vert2Frag*)vInRef0, (SrFlatShading_Vert2Frag*)vInRef1, (SrFlatShading_Vert2Frag*)vInRef2};
	SrFlatShading_Vert2Frag* outTHREE[3] = {(SrFlatShading_Vert2Frag*)vOut, (SrFlatShading_Vert2Frag*)vOut1, (SrFlatShading_Vert2Frag*)vOut2};

	float4 diffAvg = inTHREE[0]->diffuseAcc_tx + inTHREE[1]->diffuseAcc_tx + inTHREE[2]->diffuseAcc_tx;
	float4 specAvg = inTHREE[0]->specularAcc_ty + inTHREE[1]->specularAcc_ty + inTHREE[2]->specularAcc_ty;
	
	diffAvg /= 3.f;
	specAvg /= 3.f;

	outTHREE[0]->diffuseAcc_tx = diffAvg;
	outTHREE[1]->diffuseAcc_tx = diffAvg;
	outTHREE[2]->diffuseAcc_tx = diffAvg;

	outTHREE[0]->specularAcc_ty = specAvg;
	outTHREE[1]->specularAcc_ty = specAvg;
	outTHREE[2]->specularAcc_ty = specAvg;

}

void SrFlatShader::ProcessVertex( void* vOut, void* vOut1, void* vOut2, const void* vInRef0, const void* vInRef1, const void* vInRef2, const SrShaderContext* context ) const
{
	SrVertexP3N3T2* in = (SrVertexP3N3T2*)vInRef0;
	SrFlatShading_Vert2Frag* out = (SrFlatShading_Vert2Frag*)vOut;
	SrPixelShader_Constants* cBuffer = (SrPixelShader_Constants*)(context->GetPixelShaderConstantPtr());

	float3 worldpos = (context->matrixs[eMd_World] * in->pos).xyz;

	out->pos = context->matrixs[eMd_WorldViewProj] * in->pos;

	float3 normal = (context->matrixs[eMd_World].RotateVector3(in->normal));
	normal.normalize();

	float3 viewWS = context->matrixs[eMd_ViewInverse].GetTranslate() - worldpos;
	viewWS.normalize();
	
	float4 diffuseAcc = gEnv->sceneMgr->GetSkyLightColor() * (normal.y * 0.4f + 0.6f);
	float4 specularAcc = float4::make(0.f);

	CalcLights(context, worldpos, normal, viewWS, diffuseAcc, specularAcc);

	out->diffuseAcc_tx = diffuseAcc * cBuffer->difColor;
	out->specularAcc_ty = specularAcc;

	out->texcoord.xy = in->texcoord;
}

void SrFlatShader::ProcessRasterize( void* rOut, const void* rInRef0, const void* rInRef1, const void* rInRef2, float ratio, const SrShaderContext* context, bool final ) const
{
	const SrFlatShading_Vert2Frag* verA = static_cast<const SrFlatShading_Vert2Frag*>(rInRef0);
	const SrFlatShading_Vert2Frag* verB = static_cast<const SrFlatShading_Vert2Frag*>(rInRef1);
	SrFlatShading_Vert2Frag* verO = static_cast<SrFlatShading_Vert2Frag*>(rOut);

	float inv_ratio = 1.f - ratio;

	verO->pos = SrFastLerp( verA->pos, verB->pos, ratio, inv_ratio );

	verO->texcoord = SrFastLerp( verA->texcoord, verB->texcoord, ratio, inv_ratio );
	verO->diffuseAcc_tx = verA->diffuseAcc_tx;
	verO->specularAcc_ty = verA->specularAcc_ty;

	if (final)
	{
		verO->texcoord *= (1.f / verO->pos.w);
		verO->diffuseAcc_tx *= (1.f / verO->pos.w);
		verO->specularAcc_ty *= (1.f / verO->pos.w);
	}
}

void SrFlatShader::ProcessPixel( uint32* pOut, const void* pIn, const SrShaderContext* context, uint32 address ) const
{
	SrFlatShading_Vert2Frag* in = (SrFlatShading_Vert2Frag*)pIn;
	SrPixelShader_Constants* cBuffer = (SrPixelShader_Constants*)(context->GetPixelShaderConstantPtr());

	float2 texcoord = in->texcoord.xy;

	uint32 col = context->Tex2D( texcoord, 0 );
	float4 colf = uint32_2_float4(col);

	colf = colf * colf;

	colf = colf * in->diffuseAcc_tx;
	colf += cBuffer->spcColor * in->specularAcc_ty;

	colf = Clamp(colf, 0.f, 1.f);	

	colf.sqrt();

	*pOut = float4_2_uint32(colf );
}


void SrGourandShader::ProcessVertex( void* vOut, void* vOut1, void* vOut2, const void* vInRef0, const void* vInRef1, const void* vInRef2, const SrShaderContext* context ) const
{
	SrVertexP3N3T2* in = (SrVertexP3N3T2*)vInRef0;
	SrGourandShading_Vert2Frag* out = (SrGourandShading_Vert2Frag*)vOut;
	SrPixelShader_Constants* cBuffer = (SrPixelShader_Constants*)(context->GetPixelShaderConstantPtr());

	float3 worldpos = (context->matrixs[eMd_World] * in->pos).xyz;

	out->pos = context->matrixs[eMd_WorldViewProj] * in->pos;

	float3 normal = (context->matrixs[eMd_World].RotateVector3(in->normal));
	normal.normalize();

	float3 viewWS = context->matrixs[eMd_ViewInverse].GetTranslate() - worldpos;
	viewWS.normalize();


	float4 diffuseAcc = gEnv->sceneMgr->GetSkyLightColor() * (normal.y * 0.4f + 0.6f);
	float4 specularAcc = float4::make(0.f);

	CalcLights(context, worldpos, normal, viewWS, diffuseAcc, specularAcc);

	out->diffuseAcc_tx = diffuseAcc * cBuffer->difColor;
	out->specularAcc_ty = specularAcc;

	out->texcoord.xy = in->texcoord;
}

void SrGourandShader::ProcessRasterize( void* rOut, const void* rInRef0, const void* rInRef1, const void* rInRef2, float ratio, const SrShaderContext* context, bool final ) const
{
	const SrGourandShading_Vert2Frag* verA = static_cast<const SrGourandShading_Vert2Frag*>(rInRef0);
	const SrGourandShading_Vert2Frag* verB = static_cast<const SrGourandShading_Vert2Frag*>(rInRef1);
	SrGourandShading_Vert2Frag* verO = static_cast<SrGourandShading_Vert2Frag*>(rOut);

	float inv_ratio = 1.f - ratio;
	verO->pos = SrFastLerp( verA->pos, verB->pos, ratio, inv_ratio );

	verO->texcoord = SrFastLerp( verA->texcoord, verB->texcoord, ratio, inv_ratio );
	verO->diffuseAcc_tx = SrFastLerp( verA->diffuseAcc_tx, verB->diffuseAcc_tx, ratio, inv_ratio );
	verO->specularAcc_ty = SrFastLerp( verA->specularAcc_ty, verB->specularAcc_ty, ratio, inv_ratio );

	if (final)
	{
		verO->texcoord *= (1.f / verO->pos.w);
		verO->diffuseAcc_tx *= (1.f / verO->pos.w);
		verO->specularAcc_ty *= (1.f / verO->pos.w);
	}
}

void SrGourandShader::ProcessPixel( uint32* pOut, const void* pIn, const SrShaderContext* context, uint32 address ) const
{
	SrGourandShading_Vert2Frag* in = (SrGourandShading_Vert2Frag*)pIn;
	uint32* out = (uint32*)pOut;
	SrPixelShader_Constants* cBuffer = (SrPixelShader_Constants*)(context->GetPixelShaderConstantPtr());

	uint32 col = context->Tex2D( in->texcoord.xy, 0 );
	float4 colf = uint32_2_float4(col);

	colf = colf * colf;

	colf = colf * in->diffuseAcc_tx;
	colf += cBuffer->spcColor * in->specularAcc_ty;

	colf = Clamp(colf, 0.f, 1.f);	

	colf.sqrt();

	*out = float4_2_uint32(colf );
}


void SrPhongShader::ProcessVertex( void* vOut, void* vOut1, void* vOut2, const void* vInRef0, const void* vInRef1, const void* vInRef2, const SrShaderContext* context ) const
{
	SrVertexP3N3T2* in = (SrVertexP3N3T2*)vInRef0;
	SrPhongShading_Vert2Frag* out = (SrPhongShading_Vert2Frag*)vOut;

	out->pos = context->matrixs[eMd_WorldViewProj] * in->pos;

	out->worldpos_tx = (context->matrixs[eMd_World] * in->pos);
	out->worldpos_tx.w = in->texcoord.x;

	out->normal_ty = float4::make((context->matrixs[eMd_World].RotateVector3(in->normal)), in->texcoord.y);
}

void SrPhongShader::ProcessRasterize( void* rOut, const void* rInRef0, const void* rInRef1, const void* rInRef2, float ratio, const SrShaderContext* context, bool final ) const
{

	//
	// float inv_ratio = 1.f - ratio;

#ifdef SR_USE_SIMD
	// FastRasterize( (SrRendVertexAVX*)rOut, (SrRendVertexAVX*)rInRef0, (SrRendVertexAVX*)rInRef1, ratio, inv_ratio);
	//
	// if (final)
	// {
	// 	FastFinalRasterize( (SrRendVertexAVX*)rOut, ((SrRendVertex*)rOut)->pos.w);
	// }

	
	if (final)
	{
		FastRasterizeFinalSSE((SrRendVertexSSE*)rOut, (SrRendVertexSSE*)rInRef0, (SrRendVertexSSE*)rInRef1, ratio, 3);
	}
	else
	{
		FastRasterizeSSE((SrRendVertexSSE*)rOut, (SrRendVertexSSE*)rInRef0, (SrRendVertexSSE*)rInRef1, ratio, 3);
	}
#else
	const SrPhongShading_Vert2Frag* verA = static_cast<const SrPhongShading_Vert2Frag*>(rInRef0);
	const SrPhongShading_Vert2Frag* verB = static_cast<const SrPhongShading_Vert2Frag*>(rInRef1);
	SrPhongShading_Vert2Frag* verO = static_cast<SrPhongShading_Vert2Frag*>(rOut);
	// 
	verO->pos = SrFastLerp( verA->pos, verB->pos, ratio, inv_ratio );

	verO->normal_ty = SrFastLerp( verA->normal_ty, verB->normal_ty, ratio, inv_ratio );
	verO->worldpos_tx = SrFastLerp( verA->worldpos_tx, verB->worldpos_tx, ratio, inv_ratio );

	if (final)
	{
		verO->normal_ty /= verO->pos.w;
		verO->worldpos_tx /= verO->pos.w;
	}
#endif
}

void SrPhongShader::ProcessPixel( uint32* pOut, const void* pIn, const SrShaderContext* context, uint32 address ) const
{
	//return;
	SrPhongShading_Vert2Frag* in = (SrPhongShading_Vert2Frag*)pIn;
	uint32* out = (uint32*)pOut;

	SrPixelShader_Constants* cBuffer = (SrPixelShader_Constants*)(context->GetPixelShaderConstantPtr());

	float2 tc0 = float2::make( in->worldpos_tx.w, in->normal_ty.w );
	uint32 col = context->Tex2D( tc0, 0 );
	float4 matDiff = uint32_2_float4(col);
	float matSpec = matDiff.a;

	matDiff = matDiff * matDiff;

	float3 normalDir = in->normal_ty.xyz;
	normalDir.normalize();

	float3 viewWS = context->matrixs[eMd_ViewInverse].GetTranslate() - in->worldpos_tx.xyz;
	viewWS.normalize();

	float4 diffuseAcc = gEnv->sceneMgr->GetSkyLightColor();
	diffuseAcc *= (normalDir.y * 0.4f + 0.6f);
	float4 specularAcc = float4::make(0.f);

	CalcLights(context, in->worldpos_tx.xyz, normalDir, viewWS, diffuseAcc, specularAcc);
	
	diffuseAcc = diffuseAcc * matDiff * cBuffer->difColor;
	diffuseAcc += cBuffer->spcColor * specularAcc * matSpec;
	diffuseAcc = Clamp(diffuseAcc, 0.f, 1.f);	

	diffuseAcc.sqrt();

	*out = float4_2_uint32(diffuseAcc);
}

void SrPhongWithNormalShader::ProcessPatch( void* vOut, void* vOut1, void* vOut2, const void* vInRef0, const void* vInRef1, const void* vInRef2, const SrShaderContext* context ) const
{
	SrPhongShading_Vert2Frag* inTHREE[3] = {(SrPhongShading_Vert2Frag*)vInRef0, (SrPhongShading_Vert2Frag*)vInRef1, (SrPhongShading_Vert2Frag*)vInRef2};
	SrPhongShading_Vert2Frag* outTHREE[3] = {(SrPhongShading_Vert2Frag*)vOut, (SrPhongShading_Vert2Frag*)vOut1, (SrPhongShading_Vert2Frag*)vOut2};

	// calc tangent space in vertex shader!
	float3 pos1 = inTHREE[0]->tangent.xyz;
	float3 pos2 = inTHREE[1]->tangent.xyz;
	float3 pos3 = inTHREE[2]->tangent.xyz;

	float2 uv1 = float2::make(inTHREE[0]->worldpos_tx.w, inTHREE[0]->normal_ty.w);
	float2 uv2 = float2::make(inTHREE[1]->worldpos_tx.w, inTHREE[1]->normal_ty.w);
	float2 uv3 = float2::make(inTHREE[2]->worldpos_tx.w, inTHREE[2]->normal_ty.w);

	outTHREE[0]->tangent = CalculateTangentSpaceVector(pos1, pos2, pos3, uv1, uv2, uv3);
	outTHREE[1]->tangent = CalculateTangentSpaceVector(pos2, pos1, pos3, uv2, uv1, uv3);
	outTHREE[2]->tangent = CalculateTangentSpaceVector(pos3, pos2, pos1, uv3, uv2, uv1);

	outTHREE[0]->tangent.xyz = (context->matrixs[eMd_World].RotateVector3(outTHREE[0]->tangent.xyz ));
	outTHREE[1]->tangent.xyz = (context->matrixs[eMd_World].RotateVector3(outTHREE[1]->tangent.xyz ));
	outTHREE[2]->tangent.xyz = (context->matrixs[eMd_World].RotateVector3(outTHREE[2]->tangent.xyz ));
}

void SrPhongWithNormalShader::ProcessVertex( void* vOut, void* vOut1, void* vOut2, const void* vInRef0, const void* vInRef1, const void* vInRef2, const SrShaderContext* context ) const
{
	SrVertexP3N3T2* in = (SrVertexP3N3T2*)vInRef0;
	SrPhongShading_Vert2Frag* out = (SrPhongShading_Vert2Frag*)vOut;


	out->pos = context->matrixs[eMd_WorldViewProj] * in->pos;

	out->worldpos_tx = (context->matrixs[eMd_World] * in->pos);
	out->worldpos_tx.w = in->texcoord.x;

	out->normal_ty = float4::make( (context->matrixs[eMd_World].RotateVector3(in->normal)), in->texcoord.y);

	out->tangent.xyz = in->pos.xyz;
}

void SrPhongWithNormalShader::ProcessRasterize( void* rOut, const void* rInRef0, const void* rInRef1, const void* rInRef2, float ratio, const SrShaderContext* context, bool final ) const
{

	//
	// float inv_ratio = 1.f - ratio;

#ifdef SR_USE_SIMD
	if (final)
	{
		FastRasterizeFinalSSE((SrRendVertexSSE*)rOut, (SrRendVertexSSE*)rInRef0, (SrRendVertexSSE*)rInRef1, ratio, 4);
	}
	else
	{
		FastRasterizeSSE((SrRendVertexSSE*)rOut, (SrRendVertexSSE*)rInRef0, (SrRendVertexSSE*)rInRef1, ratio, 4);
	}
#else
	const SrPhongShading_Vert2Frag* verA = static_cast<const SrPhongShading_Vert2Frag*>(rInRef0);
	const SrPhongShading_Vert2Frag* verB = static_cast<const SrPhongShading_Vert2Frag*>(rInRef1);
	SrPhongShading_Vert2Frag* verO = static_cast<SrPhongShading_Vert2Frag*>(rOut);
	// 
	verO->pos = SrFastLerp(verA->pos, verB->pos, ratio, inv_ratio);

	verO->normal_ty = SrFastLerp(verA->normal_ty, verB->normal_ty, ratio, inv_ratio);
	verO->worldpos_tx = SrFastLerp(verA->worldpos_tx, verB->worldpos_tx, ratio, inv_ratio);

	if (final)
	{
		verO->normal_ty /= verO->pos.w;
		verO->worldpos_tx /= verO->pos.w;
	}
#endif

}

float2 g_reflect[16] = 
{
	float2::make( sinf( 1.25f * SR_PI / 8.f),	cosf( 1.25f * SR_PI / 8.f) ),
		float2::make( sinf( 0.25f * SR_PI / 8.f),	cosf( 0.25f * SR_PI / 8.f) ),

	float2::make( sinf( 3.25f * SR_PI / 8.f),	cosf( 3.25f * SR_PI / 8.f) ),
		float2::make( sinf( 2.25f * SR_PI / 8.f),	cosf( 2.25f * SR_PI / 8.f) ),

		float2::make( sinf( 4.25f * SR_PI / 8.f),	cosf( 4.25f * SR_PI / 8.f) ),
	float2::make( sinf( 5.25f * SR_PI / 8.f),	cosf( 5.25f * SR_PI / 8.f) ),

		float2::make( sinf( 6.25f * SR_PI / 8.f),	cosf( 6.25f * SR_PI / 8.f) ),
	float2::make( sinf( 7.25f * SR_PI / 8.f),	cosf( 7.25f * SR_PI / 8.f) ),


	float2::make( sinf( 9.25f * SR_PI / 8.f),	cosf( 9.25f * SR_PI / 8.f) ),
		float2::make( sinf( 8.25f * SR_PI / 8.f),	cosf( 8.25f * SR_PI / 8.f) ),

	float2::make( sinf( 11.25f * SR_PI / 8.f),	cosf( 11.25f * SR_PI / 8.f) ),
		float2::make( sinf( 10.25f * SR_PI / 8.f),	cosf( 10.25f * SR_PI / 8.f) ),

	float2::make( sinf( 12.25f * SR_PI / 8.f),	cosf( 12.25f * SR_PI / 8.f) ),
	float2::make( sinf( 13.25f * SR_PI / 8.f),	cosf( 13.25f * SR_PI / 8.f) ),
	float2::make( sinf( 14.25f * SR_PI / 8.f),	cosf( 14.25f * SR_PI / 8.f) ),
	float2::make( sinf( 15.25f * SR_PI / 8.f),	cosf( 15.25f * SR_PI / 8.f) ),
};

// float2 g_kernel[8] = {
// 	float2::make(-2.f,0),
// 	float2::make(2.f,0),
// 	float2::make(0,2.f),
// 	float2::make(0,-2.f),
// 	float2::make(-0.7071,0.7071),
// 	float2::make(0.7071,-0.7071),
// 	float2::make(0.7071,0.7071),
// 	float2::make(-0.7071,-0.7071),
// };

float2 g_kernel[4] = {
	float2::make(-1.4,0.7071),
	float2::make(0.7071,-1.4),
	float2::make(1.4,0.7071),
	float2::make(-0.7071,-1.4),
};

void SRFASTCALL SrPhongWithNormalShader::ProcessPixel( uint32* pOut, const void* pIn, const SrShaderContext* context, uint32 address ) const
{
	SrPhongShading_Vert2Frag* in = (SrPhongShading_Vert2Frag*)pIn;
	uint32* out = (uint32*)pOut;
	SrPixelShader_Constants* cBuffer = (SrPixelShader_Constants*)(context->GetPixelShaderConstantPtr());


	float3 worldpos = in->worldpos_tx.xyz;
	float2 tc0 = float2::make(in->worldpos_tx.w, in->normal_ty.w);
	
	uint32 col = context->Tex2D( tc0, 0 );
	float4 matDiff = uint32_2_float4(col);
	float4 matSpec = float4::make(matDiff.a);

	matDiff = matDiff * matDiff;

	float3 normalDir = in->normal_ty.xyz;
	normalDir.normalize();
	float3 tangentDir = in->tangent.xyz;
	tangentDir.normalize();
	float3 binormalDir = tangentDir % normalDir;

	float33 tangent2world( tangentDir, binormalDir, normalDir );

	uint32 nor = context->Tex2D( tc0, 1 );
	float4 norf = uint32_2_float4(nor);

	float3 normalTangent = norf.xyz;
	normalTangent = (normalTangent - float3::make(0.5f)) * 2.f;

	normalDir = tangent2world * normalTangent;
	normalDir.normalize();

	float3 viewWS = context->matrixs[eMd_ViewInverse].GetTranslate() - worldpos;
	viewWS.normalize();

	float4 diffuseAcc = gEnv->sceneMgr->GetSkyLightColor() * (normalDir.y * 0.4f + 0.6f);
	float4 specularAcc = float4::make(0.f);
	
	CalcLights(context, worldpos, normalDir, viewWS, diffuseAcc, specularAcc);

	// calc ssao here
	float ao = 0;

	bool jit = (gEnv->context->IsFeatureEnable(eRFeature_JitAA));

	float multiple = 2.f * (jit ? 2.f : 1.f);

	if ( !jit ||  ( ( (address - ((address /  gEnv->context->width) % 2)) % 2 != gEnv->renderer->getFrameCount() % 2) )  )
	{
 		int y = address / gEnv->context->width;
 		int x = address % gEnv->context->width;
		int address = x % 4 + (y % 4) * 4;
		address %= 16;
	
		float2 hTc = float2::make(in->pos.x / (float) gEnv->context->width, in->pos.y / (float) gEnv->context->height);

		// �Ż��£���sseһ������
		const int jitcount = 4;
		const float jitmultiple = 0.25f;

#ifdef SR_USE_SIMD
		float4 simdCore;
		float4 simdLength;
		__m128 zero = _mm_set1_ps(0.f);
		__m128 one = _mm_set1_ps(1.f);
		__m128 length_tweak = _mm_set1_ps(0.3f);
		__m128 final_tweak = _mm_set1_ps(multiple);
		__m128 dot_tweak = _mm_set1_ps(0.25f);
		
		for (int i = 0; i < jitcount; ++i)
		{
			float2 tc = g_kernel[i];
			tc.reflect(tc, g_reflect[address]);
			tc = (tc / (float)gEnv->context->width) * 180 * (1 - in->pos.z);
			float3 diff = gEnv->context->fBuffer->GetWorldPos(hTc + tc);
			diff -= worldpos;
			float d = diff.length();
			// if (d < SR_EQUAL_PRECISION)
			// {
			// 	//ao += 1;
			// 	continue;
			// }
			diff = diff / d;
			simdCore.m[i] = float3::dot(normalDir, diff);
			simdLength.m[i] = d;
			//d *= 0.3f;
			//ao += fmax(0.0f, ) * (1.0f / (1.0f + d)) * multiple;
		}
		simdLength.m128 = _mm_fmadd_ps(simdLength.m128, length_tweak, one);
		simdLength.m128 = _mm_div_ps(one, simdLength.m128);
		simdCore.m128 = _mm_mul_ps(_mm_mul_ps(_mm_max_ps(simdCore.m128, zero), simdLength.m128), final_tweak);

		simdCore.m128 = _mm_dot_ps(simdCore.m128, dot_tweak);

		ao = *(reinterpret_cast<float*>(&simdCore.m128));
#else
		for ( int i=0; i < jitcount; ++i)
		{
			float2 tc = g_kernel[i];
			tc.reflect( tc, g_reflect[address]);
			tc = (tc / (float)gEnv->context->width) * 180 * (1- in->pos.z);
			float3 diff = gEnv->context->fBuffer->GetWorldPos( hTc + tc );
			diff -= worldpos;
			float d = diff.length(); 
			if (d < SR_EQUAL_PRECISION)
			{
				//ao += 1;
				continue;
			}
			diff = diff / d;
			d *= 0.3f;
			ao += fmax(0.0f ,float3::dot(normalDir,diff) ) * ( 1.0f / (1.0f + d) ) * multiple;
		}

		ao *= jitmultiple;
#endif

		

		ao = Clamp(ao, 0.f, 1.f);

		diffuseAcc *= 1.f - ao;

		ao -= 0.5f;
	}

	diffuseAcc = diffuseAcc * matDiff * cBuffer->difColor;
	diffuseAcc *= (1.f - ao);
	diffuseAcc += cBuffer->spcColor * specularAcc * matSpec.x;

	diffuseAcc = Clamp(diffuseAcc, 0.f, 1.f);	

	diffuseAcc.sqrt();

	*out = float4_2_uint32(diffuseAcc);
}
