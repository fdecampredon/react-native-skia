import React, { useEffect, useMemo } from "react";
import { StyleSheet, View } from "react-native";
import { useDerivedValue, useSharedValue } from "react-native-reanimated";
import { Canvas, Rect } from "@shopify/react-native-skia";
import { Image, Skia, SkImage } from "@shopify/react-native-skia";

export function MultiThread() {

  // const jsThreadImage = useMemo(() => {
  //   const surface = Skia.Surface.MakeOffscreen(256, 256);
  //   if (surface === null) {
  //     return;
  //   }
  //   const canvas = surface.getCanvas();
  //   const paint = Skia.Paint();
  //   paint.setColor(Skia.Color("#0000FF"));
  //   canvas.drawRect({
  //     x: 0,
  //     y: 0,
  //     width: 256,
  //     height: 256,
  //   }, paint);
  //   paint.setColor(Skia.Color("#FF0000"));
  //   canvas.drawCircle(128, 128, 64, paint);
  //   surface.flush();
  //   return surface.makeImageSnapshot();
  // }, []);

  // const texture = useSharedValue<bigint | null>(null);
  // useEffect(() => {
  //   texture.value = jsThreadImage?.getBackendTexture();      
  // }, []);



  const jsThreadSurface = useMemo(() => {
    return Skia.Surface.MakeOffscreen(256, 256);
  }, []);

  const texture = useSharedValue<unknown>(null);
  useEffect(() => {
    const surface = jsThreadSurface;
    if (surface === null) {
      return;
    }
    const canvas = surface.getCanvas();
    const paint = Skia.Paint();
    paint.setColor(Skia.Color("#0000FF"));
    canvas.drawRect({
      x: 0,
      y: 0,
      width: 256,
      height: 256,
    }, paint);
    paint.setColor(Skia.Color("#FF0000"));
    canvas.drawCircle(128, 128, 64, paint);
    surface.flush();
    texture.value = jsThreadSurface?.getBackendTexture();      
  }, []);

  const image = useDerivedValue(() => {
    if (texture.value === null) {
      return null;
    }
    console.log("texture.value", texture.value);
    const image = (Skia.Image.MakeImageFromNativeTexture(texture.value, 256, 256) as SkImage);
    console.log("image", image);
    return image;
  });


  // const image = useMemo(() => 
  //   Skia.Image.MakeImageFromNativeTexture(jsThreadImage.getBackendTexture(), 256, 256).makeNonTextureImage(),
  //   [jsThreadImage]
  // );

  // const image = useMemo(() => 
  //   jsThreadImage?.makeNonTextureImage(),
  //   [jsThreadImage]
  // );
  

  return (
    <View style={style.container}>
      <Canvas style={style.canvas}>
        <Image image={image} width={256} height={256} />
      </Canvas>
    </View>
  );
}

const style = StyleSheet.create({
  container: {
    flex: 1,
  },
  canvas: {
    flex: 1,
  },
});
