Texture3D g_tNoise3D;

float Noise(float3 tex) {
  return g_tNoise3D.SampleLevel(ssTrilinearRepeat, tex*0.1, 0).x*2-1;
}

float DENSITY(float3 Position) {
  //float density = 0.5 - length(Position);
  //float density = 0;
  //float3 warp = float3(Noise(Position*0.004), Noise((Position+0.1)*0.004), Noise((Position+0.2)*0.004))*4;
  //Position += warp;
  float density = -Position.y;
  density += Noise(Position.yyy*0.26)*5.0;
  density += Noise(Position*0.125)*10.00;
  density += Noise(Position*0.49)*2.00;
  density += Noise(Position*1.01)*1.00;
  density += Noise(Position*1.96)*0.50;
  density += Noise(Position*4.03)*0.25;
  density += Noise(Position*7.97)*0.125;
  //density += Noise(Position*16.07)*0.0625;
  //density += Noise(Position*31.95)*0.03125;
  return density;
}