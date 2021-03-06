arch_get_kernel_flavour () {
	case "$SUBARCH" in
		amiga|atari|mac|bvme6000|mvme147|mvme16x|q40|sun3|sun3x)
			echo "$SUBARCH"
			return 0
		;;
		*)
			warning "Unknown $ARCH subarchitecture '$SUBARCH'."
			return 1
		;;
	esac
}

arch_check_usable_kernel () {
	# Subarchitecture must match exactly.
	if expr "$1" : ".*-$2\$" >/dev/null; then return 0; fi
	return 1
}

arch_get_kernel () {
	case "$KERNEL_MAJOR" in
		2.2)
			echo "kernel-image-$KERNEL_VERSION-$1"
			;;
		2.4)
			echo "kernel-image-$KERNEL_VERSION-$1"
			;;
		2.6)
			echo "linux-image-$KERNEL_MAJOR-$1"
			;;
		*)	warning "Unknown kernel major '$KERNEL_MAJOR'." ;;
	esac
}
