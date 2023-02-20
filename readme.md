# DirectX 12 Engine

This projects feature a basic directx12 renderer.
The goal of this sample is to implement a basic render to texture:

- First we draw a simple triangle with a varying color over time on a separate texture
- Next we bind the swap chain back buffer as the current render target
- Finally we draw a quad blurring the previous image with a simple box filter



Our descriptors heaps are separated in two entities:

- **1 RTV** (Render Target View) Heap storing our swap chain render target and our temporary textures.
- **2 SRV**  (Shader Ressource View) Heap storing for each swap chain image the current triangle color and the deffered texture.



Final Image

![ScreenShot](https://github.com/Paltoquet/DirectXRenderToTexture/doc/Capture.JPG?raw=true)
