PARTITIONS := collision_encoder collision_head gate_encoder gate_head
CORE ?= 8

.PHONY: all gvsoc flash clean checksums $(PARTITIONS)

all: $(PARTITIONS)

$(PARTITIONS):
	$(MAKE) -C partitions/$@ all CORE=$(CORE) platform=$(platform)

gvsoc:
	@for partition in $(PARTITIONS); do \
		$(MAKE) -C partitions/$$partition clean all run \
			CORE=$(CORE) platform=gvsoc DORY_CHECKSUM_HARNESS=1 || exit $$?; \
	done

# Flash one independently generated partition validation image. The camera
# runtime links the same generated sources; this target is intentionally
# explicit because flashing a head over an encoder is almost always a mistake.
flash:
	@test -n "$(PARTITION)" || \
		{ echo "usage: make flash PARTITION=<collision_encoder|collision_head|gate_encoder|gate_head>"; exit 2; }
	@case " $(PARTITIONS) " in *" $(PARTITION) "*) ;; \
		*) echo "unknown PARTITION=$(PARTITION)"; exit 2;; esac
	$(MAKE) -C partitions/$(PARTITION) all flash CORE=$(CORE) platform=board

checksums:
	sha256sum -c SHA256SUMS

clean:
	@for partition in $(PARTITIONS); do \
		$(MAKE) -C partitions/$$partition clean || exit $$?; \
	done
