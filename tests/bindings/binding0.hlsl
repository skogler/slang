//TEST:COMPARE_HLSL:-no-mangle -target dxbc-assembly -profile ps_4_0 -entry main

// Let's first confirm that Slang can reproduce what the
// HLSL compiler would already do in the simple case (when
// all shader parameters are actually used).

#ifdef __SLANG__
#define R(X) /**/
#else
#define R(X) X

#define C _SV022SLANG_parameterGroup_C
#define t _SV01t
#define s _SV01s
#define c _SV022SLANG_ParameterGroup_C1c

#endif

float4 use(float4 val) { return val; };
float4 use(Texture2D t, SamplerState s) { return t.Sample(s, 0.0); }

Texture2D 		t R(: register(t0));
SamplerState 	s R(: register(s0));

cbuffer C R(: register(b0))
{
	float c;
}

float4 main() : SV_TARGET
{
	return use(t,s) + use(c);
}