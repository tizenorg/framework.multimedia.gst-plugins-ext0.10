Name:       gst-plugins-ext0.10
Version:    0.1.1
Summary:    GStreamer extra plugins (common) Version:    1.0
Release:    1
Group:      TO_BE/FILLED_IN
License:    TO BE FILLED IN
Source0:    gst-plugins-ext0.10-0.1.1.tar.gz
BuildRequires:  pkgconfig(avsysaudio)
BuildRequires:  pkgconfig(drm-service)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(ecore-x)
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(mm-ta)
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)
BuildRequires:  pkgconfig(gstreamer-0.10)  

%description
GStreamer extra plugins (common)

%prep
%setup -q -n %{name}-%{version}


%build
export CFLAGS+=" -DGST_EXT_TIME_ANALYSIS -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "

./autogen.sh --disable-static
%configure --disable-static

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%files
%defattr(-,root,root,-)  
%{_libdir}/gstreamer-0.10/libgstavsyssink.so
%{_libdir}/gstreamer-0.10/libgstavsyssrc.so
%{_libdir}/gstreamer-0.10/libgstdrmsrc.so
%{_libdir}/gstreamer-0.10/libgstencodebin.so
%{_libdir}/gstreamer-0.10/libgstevasimagesink.so
%{_libdir}/gstreamer-0.10/libgsttoggle.so
