using UnityEngine;

namespace uWindowCapture
{
    [RequireComponent(typeof(Renderer))]
    public class UwcIconTexture : MonoBehaviour
    {
        [SerializeField] private UwcWindowTexture windowTexture_;
        
        public UwcWindowTexture windowTexture
        {
            get { return windowTexture_; }
            set
            {
                windowTexture_ = value;
                if (windowTexture_) window = windowTexture_.window;
            }
        }

        private UwcWindow window_;
        public UwcWindow window
        {
            get { return window_; }
            set
            {
                if (window_ != null)
                {
                    window_.onIconCaptured.RemoveListener(OnIconCaptured);
                    window_.SetIconTexturePtr(System.IntPtr.Zero);
                }

                window_ = value;

                if (window_ != null)
                {
                    window_.onIconCaptured.AddListener(OnIconCaptured);
                    RecreateTextureIfNeeded();
                    window_.RequestCaptureIcon();
                }
            }
        }

        public Texture2D texture { get; private set; }
        private Material material_;

        public bool isValid { get { return window != null; } }

        void Awake()
        {
            material_ = GetComponent<Renderer>().material;
        }

        void Update()
        {
            if (windowTexture != null)
            {
                if (window == null || window != windowTexture.window)
                {
                    window = windowTexture.window;
                }
            }
        }

        void OnDestroy()
        {
            if (window != null)
            {
                window.onIconCaptured.RemoveListener(OnIconCaptured);
                window.SetIconTexturePtr(System.IntPtr.Zero);
            }
            if (texture) Destroy(texture);
        }

        void RecreateTextureIfNeeded()
        {
            if (!isValid) return;

            int w = window.iconWidth;
            int h = window.iconHeight;
            if (w <= 0 || h <= 0) return;

            if (texture == null || texture.width != w || texture.height != h)
            {
                if (texture) Destroy(texture);
                
                texture = new Texture2D(w, h, TextureFormat.BGRA32, false)
                {
                    filterMode = FilterMode.Bilinear,
                    wrapMode = TextureWrapMode.Clamp
                };

                window.SetIconTexturePtr(texture.GetNativeTexturePtr());
                material_.mainTexture = texture;
            }
        }

        void OnIconCaptured()
        {
            if (!isValid) return;
            RecreateTextureIfNeeded();
            
            // Icon is static, no need to listen constantly after first successful capture
            window.onIconCaptured.RemoveListener(OnIconCaptured);
        }
    }
}