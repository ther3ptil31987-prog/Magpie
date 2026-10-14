SamplerState sam : register(s0);
Texture2D tex : register(t0);

cbuffer OverlayColor : register(b0) {
	float hdrWhiteScale;
	float3 padding;
};

float DecodeSrgb(float value) {
	return value <= 0.04045 ? value / 12.92 : pow(max((value + 0.055) / 1.055, 0), 2.4);
}

float4 main(noperspective float2 coord : TEXCOORD, noperspective float4 color : COLOR) : SV_Target {
	if (hdrWhiteScale > 0) {
		color.rgb = float3(DecodeSrgb(color.r), DecodeSrgb(color.g), DecodeSrgb(color.b)) * hdrWhiteScale;
	}
	// Coverage/alpha stays linear. The blend state performs premultiplication.
	return color * float4(1, 1, 1, tex.Sample(sam, coord).r);
}
