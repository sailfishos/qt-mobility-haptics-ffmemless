Name: qt5-feedback-haptics-ffmemless
Version: 0.1.9
Release: 1
Summary: Plugin which provides haptic feedback via ffmemless ioctl
Group: System/Plugins
License: LGPLv2.1
URL: https://github.com/nemomobile/qt-mobility-haptics-ffmemless
Source0: %{name}-%{version}.tar.bz2
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
Requires:	libprofile-qt5 >= 0.34.0.8
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt0Feedback)
BuildRequires:  libprofile-qt5-devel
Provides: qt-mobility-haptics-ffmemless > 0.1.7
Obsoletes: qt-mobility-haptics-ffmemless <= 0.1.7

%description
%{summary}.

%files
%defattr(-,root,root,-)
%{_libdir}/qt5/plugins/feedback/libqtfeedback_ffmemless.so
%{_libdir}/qt5/plugins/feedback/ffmemless.json
%{_libdir}/qt5/plugins/feedback/ffmemless.ini

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
make

%install
%qmake5_install

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig
