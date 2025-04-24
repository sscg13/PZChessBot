#include "network.hpp"
#include "incbin.h"

extern "C" {
	INCBIN(network_weights, NNUE_PATH);
}

void Network::load() {
	char *ptr = (char *)gnetwork_weightsData;
	memcpy(accumulator_weights, ptr, sizeof(accumulator_weights));
	ptr += sizeof(accumulator_weights);
	memcpy(accumulator_biases, ptr, sizeof(accumulator_biases));
	ptr += sizeof(accumulator_biases);
	memcpy(output_weights, ptr, sizeof(output_weights));
	ptr += sizeof(output_weights);
	memcpy(&output_bias, ptr, sizeof(output_bias));
}

template<bool Perspective> void Accumulator::add_weights(const int16_t* weights) {
    for (int i = 0; i < HL_SIZE; i++) {
        accumulation[Perspective][i] += weights[i];
    }
}

template<bool Perspective> void Accumulator::sub_weights(const int16_t* weights) {
    for (int i = 0; i < HL_SIZE; i++) {
        accumulation[Perspective][i] -= weights[i];
    }
}

int32_t nnue_eval(const Network &net, const Accumulator &stm, const Accumulator &ntm, uint8_t nbucket) {
	/// TODO: vectorize
	int32_t score = 0;
	for (int i = 0; i < HL_SIZE; i++) {
		int input = std::clamp((int)stm.val[i], 0, QA);
		int weight = input * net.output_weights[nbucket][i];
		score += input * weight;

		input = std::clamp((int)ntm.val[i], 0, QA);
		weight = input * net.output_weights[nbucket][HL_SIZE + i];
		score += input * weight;
	}
	score /= QA;
	score += net.output_bias[nbucket];
	score *= SCALE;
	score /= QA * QB;
	return score;
}
