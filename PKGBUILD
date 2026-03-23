# Maintainer: Maik <https://github.com/Maik-0000FF>

pkgname=fcitx5-schnelle-umlaute
pkgver=0.1.4
pkgrel=1
pkgdesc='Quick umlaut and accent input for Fcitx5 using hold-and-press gestures'
arch=('x86_64')
url='https://github.com/Maik-0000FF/schnelle-umlaute'
license=('GPL-3.0-or-later')
depends=('fcitx5')
makedepends=('cmake' 'extra-cmake-modules')
optdepends=('fcitx5-configtool: GUI configuration')
install="${pkgname}.install"
source=("${pkgname}-${pkgver}.tar.gz::${url}/archive/v${pkgver}.tar.gz")
sha256sums=('SKIP')

build() {
    cd "schnelle-umlaute-${pkgver}/addon"
    cmake -B build \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build build
}

package() {
    cd "schnelle-umlaute-${pkgver}/addon"
    DESTDIR="${pkgdir}" cmake --install build

    install -Dm644 "${srcdir}/schnelle-umlaute-${pkgver}/LICENSE" \
        "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
    install -Dm644 "${srcdir}/schnelle-umlaute-${pkgver}/README.md" \
        "${pkgdir}/usr/share/doc/${pkgname}/README.md"
}
