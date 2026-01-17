Name:           fcitx5-schnelle-umlaute
Version:        0.1.1
Release:        1%{?dist}
Summary:        Quick German umlaut input for Fcitx5

License:        GPL-3.0-or-later
URL:            https://github.com/Maik-0000FF/schnelle-umlaute
Source0:        %{url}/archive/refs/tags/v%{version}.tar.gz#/schnelle-umlaute-%{version}.tar.gz

BuildRequires:  cmake >= 3.16
BuildRequires:  extra-cmake-modules
BuildRequires:  gcc-c++
BuildRequires:  fcitx5-devel
BuildRequires:  gettext

Requires:       fcitx5
Recommends:     fcitx5-configtool

%description
Schnelle Umlaute provides quick German umlaut input using a
hold-and-wait gesture similar to PowerToys Quick Accents on Windows.

Features:
- Hold a/o/u/s + press Space to get ä/ö/ü/ß
- Configurable delays for lowercase and uppercase letters
- Multiple leader key options (Space, Arrow keys)
- Up to 20 customizable character mappings

%prep
%autosetup -n schnelle-umlaute-%{version}

%build
cd addon
cmake -B build \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DCMAKE_INSTALL_LIBDIR=%{_libdir} \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build

%install
cd addon
DESTDIR=%{buildroot} cmake --install build

%files
%license LICENSE
%doc README.md
%{_libdir}/fcitx5/schnelle-umlaute.so
%{_datadir}/fcitx5/addon/schnelle-umlaute.conf
%{_datadir}/fcitx5/addon/schnelle-umlaute.conf.in
%{_datadir}/fcitx5/addon/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml
%{_datadir}/fcitx5/inputmethod/schnelle-umlaute.conf

%post
echo ""
echo "=============================================="
echo "  Schnelle Umlaute installed successfully!"
echo "=============================================="
echo ""
echo "Setup required:"
echo ""
echo "1. Configure environment variables in ~/.bashrc or /etc/environment:"
echo "   export GTK_IM_MODULE=fcitx"
echo "   export QT_IM_MODULE=fcitx"
echo "   export XMODIFIERS=@im=fcitx"
echo ""
echo "2. LOGOUT AND LOGIN"
echo ""
echo "3. Add input method via fcitx5-configtool"
echo "   -> Input Method -> Add -> 'Schnelle Umlaute'"
echo ""
echo "4. Test: Hold 'a' + press Space -> ä"
echo ""

%changelog
* Fri Jan 17 2026 Maik <https://github.com/Maik-0000FF> - 0.1.1-1
- Initial RPM release
- Case-insensitive key release comparison for Shift+letter
- Prevent text from being sent to wrong window on focus change
