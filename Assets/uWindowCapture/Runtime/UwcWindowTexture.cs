using UnityEngine;

namespace uWindowCapture
{
	// FIX: Removed [RequireComponent(typeof(Renderer))] so this component can be used as a headless proxy for UI.
	public class UwcWindowTexture : MonoBehaviour
	{
		private bool shouldUpdateWindow_ = true;
		private bool shouldUpdateWindow
		{
			get { return shouldUpdateWindow_; }
			set
			{
				if (value && searchTiming == WindowSearchTiming.Manual) return;
				shouldUpdateWindow_ = value;
			}
		}

		public WindowSearchTiming searchTiming = WindowSearchTiming.OnlyWhenParameterChanged;

		[SerializeField] private WindowTextureType type_ = WindowTextureType.Window;
		public WindowTextureType type
		{
			get { return type_; }
			set { shouldUpdateWindow = true; type_ = value; }
		}

		[SerializeField] private bool altTabWindow_ = false;
		public bool altTabWindow
		{
			get { return altTabWindow_; }
			set { shouldUpdateWindow = true; altTabWindow_ = value; }
		}

		[SerializeField] private string partialWindowTitle_;
		public string partialWindowTitle
		{
			get { return partialWindowTitle_; }
			set { shouldUpdateWindow = true; partialWindowTitle_ = value; }
		}

		[SerializeField] private int desktopIndex_ = 0;
		public int desktopIndex
		{
			get { return desktopIndex_; }
			set { shouldUpdateWindow = true; desktopIndex_ = (UwcManager.desktopCount > 0) ? Mathf.Clamp(value, 0, UwcManager.desktopCount - 1) : 0; }
		}

		[SerializeField] private bool createChildWindows_ = true;
		public bool createChildWindows
		{
			get { return createChildWindows_; }
			set
			{
				createChildWindows_ = value;
				var childManager = GetComponent<UwcWindowTextureChildrenManager>();
				if (createChildWindows_)
				{
					if (!childManager) gameObject.AddComponent<UwcWindowTextureChildrenManager>();
				}
				else
				{
					if (childManager) Destroy(childManager);
				}
			}
		}

		public GameObject childWindowPrefab;
		public float childWindowZDistance = 0.02f;

		public CaptureMode captureMode = CaptureMode.Auto;
		public CapturePriority capturePriority = CapturePriority.Auto;
		public WindowTextureCaptureTiming captureRequestTiming = WindowTextureCaptureTiming.OnlyWhenVisible;
		public int captureFrameRate = 30;
		public bool drawCursor = true;
		public bool updateTitle = true;

		public WindowTextureScaleControlType scaleControlType = WindowTextureScaleControlType.BaseScale;
		public float scalePer1000Pixel = 1f;

		private UwcWindow window_;
		public UwcWindow window
		{
			get { return window_; }
			set
			{
				if (window_ == value) return;

				if (window_ != null)
				{
					window_.onSizeChanged.RemoveListener(OnSizeChanged);
					window_.SetWindowTexturePtr(System.IntPtr.Zero);
				}

				var oldWindow = window_;
				window_ = value;
				onWindowChanged.Invoke(window_, oldWindow);

				if (window_ != null)
				{
					shouldUpdateWindow = false;
					window_.onSizeChanged.AddListener(OnSizeChanged);
					RecreateTextureIfNeeded();
					window_.RequestCapture(CapturePriority.High);
				}
			}
		}

		public UwcWindowTextureManager manager { get; set; }
		public UwcWindowTexture parent { get; set; }

		public UwcWindowChangeEvent onWindowChanged { get; private set; } = new UwcWindowChangeEvent();

		public bool isValid { get { return window != null && window.isValid; } }
		public Texture2D texture { get; private set; }

		private Material material_;
		private Renderer renderer_;
		private MeshFilter meshFilter_;
		private float captureTimer_ = 0f;
		private bool isCaptureRequested_ = false;

		void Awake()
		{
			renderer_ = GetComponent<Renderer>();

			// FIX: Safely handle missing renderer for UI Proxy mode
			if (renderer_ != null)
			{
				material_ = renderer_.material;
			}

			meshFilter_ = GetComponent<MeshFilter>();
		}

		void Update()
		{
			UpdateSearchTiming();
			UpdateTargetWindow();

			if (!isValid)
			{
				if (renderer_) renderer_.enabled = false;
				return;
			}

			if (renderer_ && !renderer_.enabled)
			{
				renderer_.enabled = !window.isIconic && window.isVisible;
			}

			window.cursorDraw = drawCursor;
			window.captureMode = captureMode;

			UpdateScale();
			UpdateTitle();
			UpdateCaptureTimer();
			UpdateRequestCapture();
		}

		void OnDisable()
		{
			if (window != null) window.SetWindowTexturePtr(System.IntPtr.Zero);
		}

		void OnEnable()
		{
			if (window != null && texture != null)
			{
				window.SetWindowTexturePtr(texture.GetNativeTexturePtr());
			}
		}

		void OnDestroy()
		{
			if (window != null)
			{
				window.onSizeChanged.RemoveListener(OnSizeChanged);
				window.SetWindowTexturePtr(System.IntPtr.Zero);
			}

			if (texture)
			{
				Destroy(texture);
				texture = null;
			}
		}

		void OnWillRenderObject()
		{
			if (!isCaptureRequested_ || !isValid) return;

			if (captureRequestTiming == WindowTextureCaptureTiming.OnlyWhenVisible)
			{
				RequestCapture();
			}
		}

		void OnSizeChanged()
		{
			RecreateTextureIfNeeded();
		}

		void RecreateTextureIfNeeded()
		{
			if (!isValid) return;

			int w = window.textureWidth;
			int h = window.textureHeight;

			if (w <= 0 || h <= 0) return;

			if (texture == null || texture.width != w || texture.height != h)
			{
				if (texture) Destroy(texture);

				texture = new Texture2D(w, h, TextureFormat.BGRA32, false)
				{
					filterMode = FilterMode.Bilinear,
					wrapMode = TextureWrapMode.Clamp
				};

				// FIX: Safely assign material only if it exists
				if (material_ != null)
				{
					material_.mainTexture = texture;
				}

				window.SetWindowTexturePtr(texture.GetNativeTexturePtr());
			}
		}

		public void RequestCapture()
		{
			if (!isValid) return;

			isCaptureRequested_ = false;

			var priority = capturePriority;
			if (priority == CapturePriority.Auto)
			{
				priority = CapturePriority.Low;
				if (window == UwcManager.cursorWindow) priority = CapturePriority.High;
				else if (window.zOrder < UwcSetting.MiddlePriorityMaxZ) priority = CapturePriority.Middle;
			}

			window.RequestCapture(priority);
		}

		void UpdateCaptureTimer()
		{
			if (captureFrameRate < 0)
			{
				captureTimer_ = 0f;
				isCaptureRequested_ = true;
			}
			else
			{
				captureTimer_ += Time.deltaTime;
				float t = 1f / captureFrameRate;
				if (captureTimer_ < t) return;
				while (captureTimer_ > t) captureTimer_ -= t;
				isCaptureRequested_ = true;
			}
		}

		void UpdateRequestCapture()
		{
			if (isCaptureRequested_ && captureRequestTiming == WindowTextureCaptureTiming.EveryFrame)
			{
				RequestCapture();
			}
		}

		void UpdateSearchTiming()
		{
			if (searchTiming == WindowSearchTiming.Always) shouldUpdateWindow = true;
		}

		void UpdateTargetWindow()
		{
			if (!shouldUpdateWindow) return;

			switch (type)
			{
				case WindowTextureType.Window:
					window = UwcManager.Find(partialWindowTitle, altTabWindow);
					break;
				case WindowTextureType.Desktop:
					window = UwcManager.FindDesktop(desktopIndex);
					break;
			}
		}

		void UpdateTitle()
		{
			if (updateTitle && isValid) window.RequestUpdateTitle();
		}

		void UpdateScale()
		{
			// FIX: Safely check for meshFilter and sharedMesh
			if (!isValid || meshFilter_ == null || meshFilter_.sharedMesh == null) return;

			var scale = transform.localScale;
			float basePixel = 1000f / scalePer1000Pixel;

			switch (scaleControlType)
			{
				case WindowTextureScaleControlType.BaseScale:
					var extents = meshFilter_.sharedMesh.bounds.extents;
					scale.x = window.width / (extents.x * 2f * basePixel);
					scale.y = window.height / (extents.y * 2f * basePixel);
					break;
				case WindowTextureScaleControlType.FixedWidth:
					scale.y = transform.localScale.x * window.height / window.width;
					break;
				case WindowTextureScaleControlType.FixedHeight:
					scale.x = transform.localScale.y * window.width / window.height;
					break;
			}

			if (float.IsNaN(scale.x)) scale.x = 0f;
			if (float.IsNaN(scale.y)) scale.y = 0f;

			transform.localScale = scale;
		}
	}
}