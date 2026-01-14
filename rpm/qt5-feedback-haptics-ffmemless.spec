Name: qt5-feedback-haptics-ffmemless
Version: 0.2.5
Release: 1
Summary: Plugin which provides haptic feedback via ffmemless ioctl
License: LGPLv2+
URL: https://github.com/sailfishos/qt-mobility-haptics-ffmemless
Source0: %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt0Feedback)
BuildRequires:  libprofile-qt5-devel

%description
%{summary}.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
%make_build

%install
%qmake5_install

%files
%{_libdir}/qt5/plugins/feedback/libqtfeedback_ffmemless.so
%{_libdir}/qt5/plugins/feedback/ffmemless.json
%{_libdir}/qt5/plugins/feedback/ffmemless.ini
