Name:       gst-plugins-ext0.10
%if 0%{?tizen_profile_mobile}
Version:    0.3.14
Summary:    GStreamer extra plugins (common)
Release:    0
%else
Version:    0.4.13
Summary:    GStreamer extra plugins (common)
Release:    0
VCS:        framework/multimedia/gst-plugins-ext0.10#gst-plugins-ext0.10_0.3.3-8-2-125-g8a01f7e8af22f9b5977ed9bbf6e0eff94afe837b
%endif
Group:      libs
License:    LGPLv2+
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  pkgconfig(avsysaudio)
BuildRequires:  pkgconfig(camsrcjpegenc)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(ecore-x)
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(mm-ta)
BuildRequires:  pkgconfig(gstreamer-plugins-base-0.10)
BuildRequires:  pkgconfig(gstreamer-0.10)  
BuildRequires:  pkgconfig(libexif)
%if "%{_repository}" == "mobile"
BuildRequires:	pkgconfig(libdrm)
BuildRequires:	libdrm-devel
BuildRequires:	pkgconfig(libtbm)
%else
BuildRequires:  pkgconfig(libxml-2.0)
%endif
BuildRequires:	pkgconfig(libdri2)
BuildRequires:	pkgconfig(x11)
BuildRequires:	pkgconfig(xext)
BuildRequires:	pkgconfig(xv)
BuildRequires:	pkgconfig(xdamage)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:	pkgconfig(dri2proto)
BuildRequires:	pkgconfig(xfixes)

%description
GStreamer extra plugins (common)

%prep
%setup -q


%build
%if 0%{?tizen_profile_mobile}
cd mobile
%else
cd wearable
%endif

export CFLAGS+=" -DGST_EXT_TIME_ANALYSIS -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "

./autogen.sh --disable-static
%if 0%{?tizen_profile_mobile}
%configure --disable-static
%else
%configure  --disable-static \
 --disable-ext-evasimagesink\
 --disable-ext-evaspixmapsink\
 --disable-ext-xvimagesrc\
 --disable-ext-gstreamer-camera\
 --disable-ext-audiotp\
 --disable-ext-audioeq\
 --disable-ext-dashdemux\
 --disable-ext-pdpushsrc\
%endif

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}

%if 0%{?tizen_profile_mobile}
cd mobile
%else
cd wearable
%endif

mkdir -p %{buildroot}/usr/share/license
cp COPYING %{buildroot}/usr/share/license/%{name}
%make_install


%files
%if 0%{?tizen_profile_mobile}
%manifest mobile/gst-plugins-ext0.10.manifest
%else
%manifest wearable/gst-plugins-ext0.10.manifest
%endif
%defattr(-,root,root,-)  
%{_libdir}/gstreamer-0.10/*.so
%{_datadir}/license/%{name}
