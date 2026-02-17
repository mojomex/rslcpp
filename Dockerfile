ARG BASE_IMAGE=x2gen2:v4.3.1
FROM ${BASE_IMAGE}

USER autoware
WORKDIR /home/autoware

# Temp workaround for wrong Pilot Auto ccache dir
RUN sed -i '/export CCACHE_DIR="\/var\/tmp\/ccache"/d' ~/.bashrc

COPY --chown=autoware:autoware . rslcpp/

RUN cd rslcpp && \
    rm -rf build install log && \
    source ~/autoware.proj/install/setup.bash && \
    colcon build --cmake-args -DBUILD_TESTING=OFF -DCMAKE_BUILD_TYPE=Release && \
    source install/setup.bash && \
    ros2 run rslcpp_test determinism --ros-args -p use_sim_time:=true && \
    rm ./callback_execution_order.txt && \
    cd .. && \
    echo "source ~/rslcpp/install/setup.bash" >> ~/.bashrc
