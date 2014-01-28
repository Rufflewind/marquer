DIST=dist^\
TMP=$(DIST)tmp^\
CXXFLAGS=/nologo /D_CRT_SECURE_NO_WARNINGS /EHsc /Fe$(DIST) /Fo$(TMP) \
  /GL /Ox /W4
LDFLAGS=/link /MANIFEST:EMBED /MANIFESTDEPENDENCY:"type='win32' \
  name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
  processorArchitecture='*' publicKeyToken='6595b64144ccf1df'"
RFLAGS=/fo$(TMP)marquer.res
LIBS=comctl32.lib gdi32.lib gdiplus.lib user32.lib

main: $(DIST)marquer.exe $(DIST)marquer.png

$(DIST)marquer.exe: marquer.cpp $(TMP)marquer.res
	-md $(DIST)
        $(CXX) $(CXXFLAGS) $(LIBS) $(TMP)marquer.res marquer.cpp $(LDFLAGS)

$(DIST)marquer.png: marquer.png
	copy marquer.png $(DIST)marquer.png /y

$(TMP)marquer.res: resource.h marquer.rc main.ico
        -md $(TMP)
	$(RC) $(RFLAGS) marquer.rc

clean:
        -rd /s /q dist
