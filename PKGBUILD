# Maintainer: Maik <https://github.com/Maik-0000FF>

pkgname=schnelle-umlaute-fcitx5
pkgver=0.1.1
pkgrel=1
pkgdesc="Quick German umlaut input for Fcitx5 using hold and wait gesture"
arch=('x86_64')
url="https://github.com/Maik-0000FF/schnelle-umlaute"
license=('GPL-3.0-or-later')
depends=('fcitx5')
makedepends=('cmake' 'extra-cmake-modules' 'gcc')
optdepends=('fcitx5-configtool: GUI configuration tool')
source=("$pkgname-$pkgver.tar.gz::https://github.com/Maik-0000FF/schnelle-umlaute/archive/refs/tags/v$pkgver.tar.gz")
sha256sums=('SKIP')

build() {
    cd "$srcdir/schnelle-umlaute-$pkgver/addon"

    cmake -B build \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release

    cmake --build build
}

package() {
    cd "$srcdir/schnelle-umlaute-$pkgver/addon"

    DESTDIR="$pkgdir" cmake --install build

    # Install documentation
    install -Dm644 "$srcdir/schnelle-umlaute-$pkgver/README.md" \
        "$pkgdir/usr/share/doc/$pkgname/README.md"
}

post_install() {
    echo ""
    echo "Schnelle Umlaute has been installed!"
    echo ""
    echo "IMPORTANT: Setup required for full functionality:"
    echo ""
    echo "1. Configure environment variables:"
    echo "   mkdir -p ~/.config/environment.d"
    echo "   cat > ~/.config/environment.d/fcitx5.conf << 'EOF'"
    echo "GTK_IM_MODULE=fcitx5"
    echo "QT_IM_MODULE=fcitx5"
    echo "XMODIFIERS=@im=fcitx5"
    echo "GLFW_IM_MODULE=ibus"
    echo "EOF"
    echo ""
    echo "2. LOGOUT AND LOGIN for changes to take effect"
    echo ""
    echo "3. Add input method:"
    echo "   fcitx5-config-qt"
    echo "   -> Input Method -> Add -> Search 'Schnelle Umlaute'"
    echo ""
    echo "4. Switch to it using Ctrl+Space and test:"
    echo "   Hold 'a' + press Space -> ä"
    echo ""
    echo "See /usr/share/doc/schnelle-umlaute-fcitx5/README.md for details"
    echo ""
}

post_upgrade() {
    post_install
}
