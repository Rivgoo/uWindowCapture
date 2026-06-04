using UnityEngine;

namespace uWindowCapture
{
    public class UwcCursor
    {
        public int x { get { return Lib.GetCursorX(); } }
        public int y { get { return Lib.GetCursorY(); } }
        public int width { get { return Lib.GetCursorWidth(); } }
        public int height { get { return Lib.GetCursorHeight(); } }

        public Texture2D texture { get; private set; }

        public UwcEvent onCaptured { get; private set; } = new UwcEvent();
        public UwcEvent onTextureChanged { get; private set; } = new UwcEvent();

        public void RequestCapture()
        {
            Lib.RequestCaptureCursor();
        }

        public void CreateTextureIfNeeded()
        {
            int w = width;
            int h = height;
            if (w == 0 || h == 0) return;

            if (texture == null || texture.width != w || texture.height != h)
            {
                if (texture) Object.Destroy(texture);
                
                texture = new Texture2D(w, h, TextureFormat.BGRA32, false)
                {
                    filterMode = FilterMode.Point,
                    wrapMode = TextureWrapMode.Clamp
                };

                // Forward Native Pointer to C++ for Direct Copy
                Lib.SetCursorTexturePtr(texture.GetNativeTexturePtr());
                onTextureChanged.Invoke();
            }
        }
    }
}