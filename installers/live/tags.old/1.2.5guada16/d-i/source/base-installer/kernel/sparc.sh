arch_get_kernel_flavour () {
	case "$MACHINE" in
		sparc)
			echo sparc32
			log 'sparc32 not supported'
			return 1
		;;
		sparc64)	echo sparc64 ;;
	esac
	return 0
}

arch_check_usable_kernel () {
	case "$2" in
	    sparc32)
		if expr "$1" : '.*-2\.6.*-sparc32-smp' >/dev/null; then
			# No working SMP yet
			return 1
		fi
		if expr "$1" : '.*-sparc32.*' >/dev/null; then return 0; fi
		return 1
		;;
	    sparc64)
		if expr "$1" : '.*-sparc64.*' >/dev/null; then return 0; fi
		return 1
		;;
	esac

	# default to usable in case of strangeness
	warning "Unknown kernel usability: $1 / $2"
	return 0
}

arch_get_kernel () {
	CPUS=`grep 'ncpus probed' "$CPUINFO" | cut -d: -f2`
	if [ "$CPUS" -ne 1 ]; then
		if [ "$1" = sparc32 ] && [ "$KERNEL_MAJOR" = 2.6 ]; then
			# No working SMP yet
			:
		else
			echo "linux-$1-smp"
			echo "linux-image-$1-smp"
		fi
	fi
	echo "linux-$1"
	echo "linux-image-$1"
}
