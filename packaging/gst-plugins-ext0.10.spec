Name:       gst-plugins-ext0.10
Version:    0.3.11
Summary:    GStreamer extra plugins (common)
Release:    0
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
BuildRequires:	pkgconfig(libdri2)
BuildRequires:	pkgconfig(x11)
BuildRequires:	pkgconfig(xext)
BuildRequires:	pkgconfig(xv)
BuildRequires:	pkgconfig(xdamage)
BuildRequires:	pkgconfig(libdrm)
BuildRequires:  pkgconfig(libtbm)
BuildRequires:	libdrm-devel
BuildRequires:	pkgconfig(dri2proto)
BuildRequires:	pkgconfig(xfixes)
BuildRequires:	pkgconfig(libtbm)

%description
GStreamer extra plugins (common)

%prep
%setup -q


%build
export CFLAGS+=" -DGST_EXT_TIME_ANALYSIS -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" "

./autogen.sh --disable-static
%configure --disable-static

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}/usr/share/license
cp LICENSE.LGPLv2.1 %{buildroot}/usr/share/license/%{name}
%make_install


%files
%manifest gst-plugins-ext0.10.manifest
%defattr(-,root,root,-)  
%{_libdir}/gstreamer-0.10/*.so
%{_datadir}/license/%{name}
