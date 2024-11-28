import React, { useEffect, useRef } from "react";
import { StyleSheet, View } from "react-native";
import { useDerivedValue, useSharedValue } from "react-native-reanimated";
import { Canvas } from "react-native-wgpu";
import { Image, Skia, SkImage } from "@shopify/react-native-skia";

export function MultiThread() {


  const texture = useSharedValue<bigint | null>(null);
  const image = useDerivedValue(() => {
    if (texture.value === null) {
      return null;
    }
    return (Skia.Image.MakeImageFromNativeTexture(texture.value, 256, 256) as SkImage);
  });

  let snapshot = useRef<SkImage | null>(null);

  useEffect(() => {
    const surface = Skia.Surface.MakeOffscreen(256, 256);
    const canvas = surface?.getCanvas();
    canvas?.drawColor(Skia.Color("#FF0000"));
    surface?.flush();
    const image = surface?.makeImageSnapshot();
    snapshot.current = image ?? null;
    texture.value = image?.getBackendTexture();      
  }, []);


  return (
    <View style={style.container}>
      <Canvas>
        <Image image={image} width={256} height={256} />
      </Canvas>
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
  },
  webgpu: {
    flex: 1,
  },
});
