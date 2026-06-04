using UnityEngine;

namespace uWindowCapture
{
	public class UwcWindow
	{
		public int id { get; private set; }
		public UwcWindow parentWindow { get; private set; }
		public bool isAlive { get; set; }

		public System.IntPtr handle { get { return Lib.GetWindowHandle(id); } }
		public System.IntPtr ownerHandle { get { return Lib.GetWindowOwnerHandle(id); } }
		public System.IntPtr parentHandle { get { return Lib.GetWindowParentHandle(id); } }
		public System.IntPtr instance { get { return Lib.GetWindowInstance(id); } }
		public int processId { get { return Lib.GetWindowProcessId(id); } }
		public int threadId { get { return Lib.GetWindowThreadId(id); } }

		public bool isValid { get { return Lib.CheckWindowExistence(id); } }
		public bool isRoot { get { return parentWindow == null; } }
		public bool isChild { get { return !isRoot; } }

		public bool isVisible { get { return Lib.IsWindowVisible(id); } }
		public bool isAltTabWindow { get { return Lib.IsAltTabWindow(id); } }
		public bool isDesktop { get { return Lib.IsDesktop(id); } }
		public bool isEnabled { get { return Lib.IsWindowEnabled(id); } }
		public bool isUnicode { get { return Lib.IsWindowUnicode(id); } }
		public bool isZoomed { get { return Lib.IsWindowZoomed(id); } }
		public bool isMaximized { get { return isZoomed; } }
		public bool isIconic { get { return Lib.IsWindowIconic(id); } }
		public bool isMinimized { get { return isIconic; } }
		public bool isHungup { get { return Lib.IsWindowHungUp(id); } }
		public bool isTouchable { get { return Lib.IsWindowTouchable(id); } }
		public bool isApplicationFrameWindow { get { return Lib.IsApplicationFrameWindow(id); } }
		public bool isUWP { get { return Lib.IsWindowUWP(id); } }
		public bool isBackground { get { return Lib.IsWindowBackground(id); } }

		public string title { get { return Lib.GetWindowTitle(id); } }
		public string className { get { return Lib.GetWindowClassName(id); } }

		public int rawX { get { return Lib.GetWindowX(id); } }
		public int rawY { get { return Lib.GetWindowY(id); } }
		public int rawWidth { get { return Lib.GetWindowWidth(id); } }
		public int rawHeight { get { return Lib.GetWindowHeight(id); } }

		public int textureOffsetX { get { return Lib.GetWindowTextureOffsetX(id); } }
		public int textureOffsetY { get { return Lib.GetWindowTextureOffsetY(id); } }

		public int x { get { return rawX + textureOffsetX; } }
		public int y { get { return rawY + textureOffsetY; } }

		// Logical width and height (from bounds)
		public int width { get { return rawWidth; } }
		public int height { get { return rawHeight; } }

		// These return the dimensions that C++ expects the Unity Texture2D to be.
		public int textureWidth { get { return Lib.GetWindowTextureWidth(id); } }
		public int textureHeight { get { return Lib.GetWindowTextureHeight(id); } }

		public int iconWidth { get { return Lib.GetWindowIconWidth(id); } }
		public int iconHeight { get { return Lib.GetWindowIconHeight(id); } }
		public int zOrder { get { return Lib.GetWindowZOrder(id); } }

		public CaptureMode captureMode
		{
			get { return Lib.GetWindowCaptureMode(id); }
			set { Lib.SetWindowCaptureMode(id, value); }
		}

		public bool cursorDraw
		{
			get { return Lib.GetWindowCursorDraw(id); }
			set { Lib.SetWindowCursorDraw(id, value); }
		}

		public UwcEvent onCaptured { get; private set; } = new UwcEvent();
		public UwcEvent onSizeChanged { get; private set; } = new UwcEvent();
		public UwcEvent onIconCaptured { get; private set; } = new UwcEvent();

		public class ChildAddedEvent : UnityEngine.Events.UnityEvent<UwcWindow> { }
		public ChildAddedEvent onChildAdded { get; private set; } = new ChildAddedEvent();

		public class ChildRemovedEvent : UnityEngine.Events.UnityEvent<UwcWindow> { }
		public ChildRemovedEvent onChildRemoved { get; private set; } = new ChildRemovedEvent();

		public UwcWindow(int id)
		{
			this.id = id;
			isAlive = true;

			parentWindow = UwcManager.FindParent(id);
			if (parentWindow != null)
			{
				parentWindow.onChildAdded.Invoke(this);
			}
		}

		public void RequestUpdateTitle()
		{
			Lib.RequestUpdateWindowTitle(id);
		}

		public void RequestCaptureIcon()
		{
			Lib.RequestCaptureIcon(id);
		}

		public void RequestCapture(CapturePriority priority = CapturePriority.High)
		{
			Lib.RequestCaptureWindow(id, priority);
		}

		public void SetWindowTexturePtr(System.IntPtr ptr)
		{
			Lib.SetWindowTexturePtr(id, ptr);
		}

		public void SetIconTexturePtr(System.IntPtr ptr)
		{
			Lib.SetWindowIconTexturePtr(id, ptr);
		}

		public Color32[] GetPixels(int x, int y, int w, int h)
		{
			return Lib.GetWindowPixels(id, x, y, w, h);
		}

		public bool GetPixels(Color32[] colors, int x, int y, int w, int h)
		{
			return Lib.GetWindowPixels(id, colors, x, y, w, h);
		}

		public Color32 GetPixel(int x, int y)
		{
			return Lib.GetWindowPixel(id, x, y);
		}
	}
}