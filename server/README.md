# Monitoring Server

**Responsibilities:**
- Store metrics
- Provide metrics

**Type of metrics:**
- Simple value (dot), like amount of registred users
- Derivative (speed), like rps.
$$
dot_i = \frac{value_i}{t_i - t_{i - 1}},\space |diff| = seconds
$$
