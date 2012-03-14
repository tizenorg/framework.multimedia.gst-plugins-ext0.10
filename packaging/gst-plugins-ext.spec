Name:       gst-plugins-ext
Version:    0.1.5
Summary:    GStreamer extra plugins
Release:    1
Group:      TO_BE/FILLED_IN
License:    TO BE FILLED IN
Source0:    gst-plugins-ext-%{version}.tar.gz
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

./autogen.sh 
%configure 

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%files
%{_libdir}/gstreamer-0.10/*.so
