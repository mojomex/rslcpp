FROM x2gen2:v4.3.1

USER root

# Install colcon-mixin support and bash-completion
RUN apt-get update && apt-get install -y --no-install-recommends \
    python3-colcon-mixin \
    bash-completion \
    python3-vcstool \
    vim \
    less \
    && rm -rf /var/lib/apt/lists/*

USER autoware
WORKDIR /home/autoware

# Source ROS and completions in bashrc, make mixins available
RUN echo "source /home/autoware/autoware.proj/install/setup.bash" >> ~/.bashrc && \
    echo "source /usr/share/colcon_argcomplete/hook/colcon-argcomplete.bash" >> ~/.bashrc && \
    echo "source /usr/share/vcstool-completion/vcs.bash" >> ~/.bashrc && \
    colcon mixin add default https://raw.githubusercontent.com/colcon/colcon-mixin-repository/master/index.yaml && \
    colcon mixin update default
