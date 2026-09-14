# rigor-certicoq sidecar — L-RESEARCH-SIDECAR-2026-09-12
#
# Research / evaluation / education only.
# CompCert 3.13.1 is INRIA Non-Commercial. Do not vendor this image
# into git. Do not ship ccomp as a production binary.
#
# Build (from formal_verification/rigor_kernel):
#   ./certicoq/scripts/build_sidecar.sh
#
# The image is not on the host PATH. Cone /usr/bin/coqc stays 8.15.0.

FROM coqorg/coq:8.19.2-ocaml-4.14.2-flambda

LABEL org.opencontainers.image.title="rigor-certicoq"
LABEL org.opencontainers.image.description="Coq 8.19.2 + CertiCoq 0.9+8.19 + CompCert 3.13.1 research sidecar"
LABEL rigor.decision="L-RESEARCH-SIDECAR-2026-09-12"
LABEL rigor.compcert.license="INRIA Non-Commercial License Agreement"
LABEL rigor.cone.coq="8.15.0 (host; not this image)"

USER root
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
         clang \
         ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY opam.pinned /opt/rigor/opam.pinned
COPY ci/NOTICE.compcert /opt/rigor/NOTICE.compcert

USER coq
WORKDIR /home/coq

# Print the NC notice in the build log (CertiCoqToolchain.md §1.7).
RUN echo "COMPCERT_LICENSE=INRIA_NON_COMMERCIAL" \
    && echo "COMPCERT_PATH=L-RESEARCH-SIDECAR-2026-09-12" \
    && cat /opt/rigor/NOTICE.compcert \
    && (opam repo list --short | grep -Eqx 'rocq-released|coq-released' \
        || opam repo add rocq-released https://rocq-prover.org/opam/released) \
    && opam update \
    && opam pin add -n -y -k version coq-compcert 3.13.1 \
    && opam pin add -n -y -k version coq-certicoq 0.9+8.19 \
    && opam pin add -n -y -k version coq-equations 1.3+8.19 \
    && eval "$(opam env)" \
    && opam install -y --confirm-level=unsafe-yes \
         $(grep -v '^#' /opt/rigor/opam.pinned | grep -v '^coq\.8\.19') \
    && command -v ccomp \
    && ccomp -version \
    && echo "SIDECAR_CCOMP=PRESENT" \
    && echo "SIDECAR_COQ=$(coqc --print-version | awk '{print $1}')"

# PATH inside the image only. Never bind this to the host cone PATH.
ENV COMPCERT_LICENSE=INRIA_NON_COMMERCIAL
ENV COMPCERT_PATH=L-RESEARCH-SIDECAR-2026-09-12
ENV RIGOR_SIDECAR=1
