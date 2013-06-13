Name: qt-mobility-haptics-ffmemless
Version: 0.0.7
Release: 1
Summary: Plugin which provides haptic feedback via ffmemless ioctl
Group: System/Plugins
License: LGPLv2.1
URL: https://github.com/nemomobile/qt-mobility-haptics-ffmemless
Source0: %{name}-%{version}.tar.bz2
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(QtCore)
BuildRequires:  pkgconfig(QtFeedback)

%description
%{summary}.

%files
%defattr(-,root,root,-)
%{_libdir}/qt4/plugins/feedback/libqtfeedback_ffmemless.so
%{_libdir}/qt4/plugins/feedback/ffmemless.ini

%prep
%setup -q -n %{name}-%{version}

%build
%qmake
make

%install
%qmake_install

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig
