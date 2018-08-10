#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <oqs/oqs.h>

struct sig_kat {
	enum OQS_SIG_algid algid;
};

/* Add new testcases here */
struct sig_kat sig_kats[] = {
#ifdef ENABLE_SIG_PICNIC
    {OQS_SIG_algid_picnic_L1_FS},
    {OQS_SIG_algid_picnic_L1_UR},
    {OQS_SIG_algid_picnic_L3_FS},
    {OQS_SIG_algid_picnic_L3_UR},
    {OQS_SIG_algid_picnic_L5_FS},
    {OQS_SIG_algid_picnic_L5_UR},
#endif
#ifdef ENABLE_SIG_QTESLA
    {OQS_SIG_algid_qTESLA_I},
    {OQS_SIG_algid_qTESLA_III_speed},
    {OQS_SIG_algid_qTESLA_III_size},
#endif
};

void fprintBstr(FILE *fp, const char *S, const uint8_t *A, size_t L) {
	size_t i;
	fprintf(fp, "%s", S);
	for (i = 0; i < L; i++) {
		fprintf(fp, "%02X", A[i]);
	}
	if (L == 0) {
		fprintf(fp, "00");
	}
	fprintf(fp, "\n");
}

static OQS_STATUS sig_kat(enum OQS_SIG_algid algid) {

	uint8_t entropy_input[48];
	uint8_t seed[48];
	FILE *fh = NULL;
	OQS_SIG *sig = NULL;
	size_t mlen = 33, smlen;
	uint8_t *public_key = NULL;
	uint8_t *secret_key = NULL;
	uint8_t *message = NULL;
	uint8_t *signed_message = NULL;
	char filename[200];
	OQS_STATUS rc, ret = OQS_ERROR;

	sig = OQS_SIG_new(algid);
	if (sig == NULL) {
		printf("[sig_kat] %s was not enabled at compile-time.\n", sig->method_name);
		goto err;
	}

	for (size_t i = 0; i < 48; i++) {
		entropy_input[i] = i;
	}

	rc = OQS_randombytes_switch_algorithm(OQS_RAND_alg_nist_kat);
	if (rc != OQS_SUCCESS) {
		goto err;
	}
	OQS_randombytes_nist_kat_init(entropy_input, NULL, 256);

	sprintf(filename, "kat_sig_rsp/%s.kat", sig->method_name);

	fh = fopen(filename, "w");
	if (fh == NULL) {
		goto err;
	}

	fprintf(fh, "count = 0\n");
	OQS_randombytes(seed, 48);
	fprintBstr(fh, "seed = ", seed, 48);

	fprintf(fh, "mlen = %lu\n", mlen);
	public_key = malloc(sig->pub_key_len);
	secret_key = malloc(sig->priv_key_len);
	message = malloc(mlen + sig->max_sig_len);
	signed_message = malloc(mlen + sig->max_sig_len);
	if ((public_key == NULL) || (secret_key == NULL) || (message == NULL) || (signed_message == NULL)) {
		fprintf(stderr, "[kat_sig] %s ERROR: malloc failed!\n", sig->method_name);
		goto err;
	}
	OQS_randombytes(message, mlen);
	fprintBstr(fh, "msg = ", message, mlen);

	OQS_randombytes_nist_kat_init(seed, NULL, 256);

	rc = OQS_SIG_keygen(sig, secret_key, public_key);
	if (rc != OQS_SUCCESS) {
		fprintf(stderr, "[kat_sig] %s ERROR: OQS_SIG_keypair failed!\n", sig->method_name);
		goto err;
	}
	fprintBstr(fh, "pk = ", public_key, sig->pub_key_len);
	fprintBstr(fh, "sk = ", secret_key, sig->priv_key_len);

	rc = OQS_SIG_sign(sig, secret_key, message, mlen, signed_message, &smlen);
	if (rc != OQS_SUCCESS) {
		fprintf(stderr, "[kat_sig] %s ERROR: OQS_SIG_sign failed!\n", sig->method_name);
		goto err;
	}
	fprintf(fh, "smlen = %lu\n", smlen);
	fprintBstr(fh, "sm = ", signed_message, smlen);

	rc = OQS_SIG_verify(sig, public_key, message, mlen, signed_message, smlen);
	if (rc != OQS_SUCCESS) {
		fprintf(stderr, "[kat_sig] %s ERROR: OQS_SIG_verify failed!\n", sig->method_name);
		goto err;
	}

	ret = OQS_SUCCESS;
	goto cleanup;

err:
	ret = OQS_ERROR;

cleanup:
	if (fh != NULL) {
		fclose(fh);
	}
	if (sig != NULL) {
		OQS_MEM_secure_free(secret_key, sig->priv_key_len);
	}
	OQS_MEM_insecure_free(public_key);
	OQS_MEM_insecure_free(message);
	OQS_MEM_insecure_free(signed_message);
	OQS_SIG_free(sig);
	return ret;
}

int main() {

	int ret = EXIT_SUCCESS;
	OQS_STATUS rc;
	size_t sig_kats_len = sizeof(sig_kats) / sizeof(struct sig_kat);

	int status;
	status = mkdir("kat_sig_rsp", S_IRWXU);
	if (!((status == 0) || (errno == EEXIST))) {
		return EXIT_FAILURE;
	}

	for (size_t i = 0; i < sig_kats_len; i++) {
		rc = sig_kat(sig_kats[i].algid);
		if (rc != OQS_SUCCESS) {
			ret = EXIT_FAILURE;
		}
	}

	return ret;
}
