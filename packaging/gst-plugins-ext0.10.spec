Name:       gst-plugins-ext0.10
Version:     0.2.4
Summary:  GStreamer extra plugins (common)
Release:    1
Group:      TO_BE/FILLED_IN
License:    TO BE FILLED IN
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
%make_install


%files
%defattr(-,root,root,-)  
%{_libdir}/gstreamer-0.10/*.so
