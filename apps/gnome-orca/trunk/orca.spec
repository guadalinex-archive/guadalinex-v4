Summary: A screen reader that provides access to the GNOME desktop by people with visual impairments
Name: orca
Version: 2.21.0
Release: 1
License: LGPL
Group: Desktop/Accessibility
URL: http://www.gnome.org/
Source0: orca-%{version}.tar.gz

%define python_version 2.4
%define pyorbit_version 2.0.1
%define pygtk2_version 2.6.2
%define gnome_python_version 2.6.2
%define brltty_version 3.7.2
%define atk_version 1.11.3
%define gail_version 1.8.11
%define libgail_version 1.1.3
%define gnome_speech_version 0.3.10
%define eel_version 2.14.0
%define libspi_version 1.7.6

BuildRoot: %{_tmppath}/orca-%{version}-%{release}-buildroot
BuildRequires:	gnome-common >= 2.12.0
BuildRequires:  python >= %{python_version}
BuildRequires:  pygtk2 >= %{pygtk2_version}
BuildRequires:  pyorbit >= %{pyorbit_version}
BuildRequires:	atk >= %{atk_version}
BuildRequires:	gail >= %{gail_version}
BuildRequires:	eel-2.0 >= %{eel_version}
BuildRequires:	libspi-1.0 >= %{libspi_version}
BuildRequires:	gnome-speech-1.0 >= ${gnome_speech_version}
Requires:       python >= %{python_version}
Requires:       pygtk2 >= %{pygtk2_version}
Requires:       pyorbit >= %{pyorbit_version}
Requires:	atk >=  %{atk_version}
Requires:	gail >=  %{gail_version}
Requires:	libgail-gnome >= %{libgail_version}
Requires:	eel-2.0 >= %{eel_version}
Requires:	libspi-1.0 >= %{libspi_version}
Requires:	gnome-speech-1.0 >= ${gnome_speech_version}

%description
A flexible, scriptable, extensible screen reader for the GNOME platform
that provides access via speech synthesis, braille, and magnification.

%prep
%setup -q

%build
%configure
make

%install
rm -rf $RPM_BUILD_ROOT
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc
%{_bindir}/orca
%{_libdir}/python?.?/site-packages/orca
%{_datadir}/locale/*/*
%{_datadir}/applications/orca.desktop
%{_datadir}/icons/hicolor/48x48/apps/orca.png

%changelog
* Sat Feb 03 2007 Willie Walker <william.walker@sun.com>
- Add libgail-gnome dependency.
* Thu Aug 03 2006 Willie Walker <william.walker@sun.com>
- Add orca.png and orca.desktop files.
* Tue Jul 11 2006 Willie Walker <william.walker@sun.com>
- Fix "pyborit" typo.
* Mon May 15 2006 Willie Walker <william.walker@sun.com>
- More tweaking.
* Fri May 12 2006 Willie Walker <william.walker@sun.com>
- Well, try again.  Third time is a charm?
* Thu May 04 2006 Willie Walker <william.walker@sun.com>
- One more attempt at getting the dependencies right.
* Tue Apr 25 2006 Willie Walker <william.walker@sun.com>
- Update for new preferences settings mechanism (remove orca-setup.in)
* Mon Oct 17 2005 Willie Walker <william.walker@sun.com>
- Update for 0.2.0.
