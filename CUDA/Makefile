CXX ?= g++
CXXFLAGS ?= -O2 -Wall -Wextra -Wpedantic
INCLUDES = -Ihost -Icommon -Idevice

BUILDDIR = build

all: $(BUILDDIR)/libefacudadp.so

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/libefacudadp.so: $(BUILDDIR)/efa_cuda_dp.o | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -shared -o $@ $^

$(BUILDDIR)/efa_cuda_dp.o: host/efa_cuda_dp.cpp host/efa_cuda_dp.h \
			   common/efa_cuda_dp_types.h \
			   common/efa_cuda_dp_version.h device/efa_io_defs.h | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -fPIC -c $< -o $@

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean
